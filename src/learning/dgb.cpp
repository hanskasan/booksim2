// Hans Kasan
// CSNL-KAIST
// DGB for routing

#ifdef DGB_ON
#include <assert.h>
#include <limits>

#include "dgb.hpp"

DGB::DGB(int rID, int outputs, int vcs, int max_local_buff, int max_global_buff)
{
    _rID = rID;
    _outputs = outputs;
    _vcs = vcs;

    _num_routers = gNodes / gC;

    // BUFFER SIZE FOR EXPLORATION
    _max_local_buff = max_local_buff;
    if (max_global_buff == -1)
        _max_global_buff = max_local_buff;
    else 
        _max_global_buff = max_global_buff;

    // USING LITTLE'S LAW AS LOCAL INFORMATION
    _outport_arrival.resize((_outputs - gC), deque<int>(_vcs));
    _waiting_flit.resize(_outputs - gC, 0);
    _waiting_flit_dq.resize(_outputs - gC);

    // FOR LOCAL INFORMATION
    _local_contention.resize(_outputs - gC, vector<deque<pair<int, int > > >(2));

    //_min_local_latency.resize(_num_routers, 0);
    _min_local_latency_deque.resize(_num_routers);
    _non_local_latency.resize(_num_routers, vector<int>(_outputs - gC, 0));

    //_n_min_local_latency.resize(_num_routers, 0);
    _n_non_local_latency.resize(_num_routers, vector<int>(_outputs - gC, 0));

    // FOR ESTIMATING UTILIZATION FROM BIAS
    _estm_vect.resize((_outputs - gC), vector<vector<queue<int> > >(N_ACTIONS ,vector<queue<int> >(2)));

    // FOR ASSIGNING VALUE FOR EACH BIAS
    _bias_weight.resize(_num_routers, vector<float>(N_ACTIONS, 1.0));

    // FOR GLOBAL INFORMATION FEEDBACK
    _my_min_diff_vect.resize(_num_routers);
    _my_non_diff_vect.resize(_num_routers, vector<deque<pair<int, float> > >(_outputs - gC));
    //_my_non_diff_vect.resize(_num_routers, vector<deque<pair<int, float> > >(32));

    _min_diff_vect.resize(_num_routers, 0.0);
    _n_min_diff_vect.resize(_num_routers, 0);
    //_min_arrived_vect.resize(_num_routers, 0);

    if (gIsDragonfly){
        _non_diff_vect.resize(_num_routers, vector<vector<float> >(_outputs - gC, vector<float>(gK, 0.0)));
        _n_non_diff_vect.resize(_num_routers, vector<vector<int> >(_outputs - gC, vector<int>(gK, 0)));
    } else {
        if (gN == 1){
            _non_diff_vect.resize(_num_routers, vector<vector<float> >(_outputs - gC, vector<float>(1, 0.0)));
            _n_non_diff_vect.resize(_num_routers, vector<vector<int> >(_outputs - gC, vector<int>(1, 0)));
        } else {    
            assert(gN == 2); // HANS: Still not sure about higher dimensional networks, maybe should use power instead of multiplication
            _non_diff_vect.resize(_num_routers, vector<vector<float> >(_outputs - gC, vector<float>(gK * (gN - 1), 0.0)));
            _n_non_diff_vect.resize(_num_routers, vector<vector<int> >(_outputs - gC, vector<int>(gK * (gN - 1), 0)));
        }
       
    }

#ifdef DGB_PIGGYBACK
    // FOR GLOBAL QUEUE INFORMATION PIGGYBACK
    _globalq_vect.resize(2 * gK * gK, -1);
    _globalqnet_vect.resize(2 * gK * gK, -1);
#endif

    // FOR TRAINING
    _bias_vect.resize(_num_routers, vector<float>(_outputs - gC, 0.0));
    _explore_vect.resize(_num_routers, vector<bool>(_outputs - gC, false));
    _last_localtrain_time.resize(_num_routers, vector<int>(_outputs - gC, 0));
    _is_train.resize(_num_routers, false);

    //_prev_local_cost.resize(_outputs - gC, -1.0);
    _prev_local_lat.resize(_outputs - gC, 0);
    _prev_local_lat_count.resize(_outputs - gC, 0);
    _latest_learnset_gen_time.resize(_num_routers, 0);

    //_max_exploration_bound.resize(_num_routers, 0.0);

    // FOR RESETTING
    //_prev_all_avg.resize(_num_routers, 0.0);
    //_prev_all_avg_initialized.resize(_num_routers, false);

    // DEPARTURE COUNTERS
    _min_departure_counter.resize(_num_routers, 0);
    _non_departure_counter.resize(_num_routers, vector<int>(_outputs - gC, 0));

    // HYPERPARAMETERS
    EPOCH_SIZE = 50;
    EXPLORE_RATE = 0.25;
    LEARNING_RATE = 0.25;
}

DGB::~DGB()
{
}

// USING LITTLE'S LAW AS LOCAL INFORMATION
// void DGB::RegisterService(int out_port, int time)
// {
//     assert((out_port >= gC) && (out_port < _outputs));
//     assert(time >= 0);

//     _outport_arrival[out_port - gC].push_back(GetSimTime());
// }

float DGB::GetArrivalRate(int out_port)
{
    assert((out_port >= gC) && (out_port < _outputs));

    // Retire old data
    while ((!_outport_arrival[out_port - gC].empty()) && ((_outport_arrival[out_port - gC].front() + EPOCH_SIZE) < GetSimTime())){
		_outport_arrival[out_port - gC].pop_front();
	}

    int size = _outport_arrival[out_port - gC].size();
    
    return ((float)size / (float)EPOCH_SIZE);
}

void DGB::IncrementWaitingFlit(int out_port)
{
    assert((out_port >= gC) && (out_port < _outputs));
    _waiting_flit[out_port - gC] += 1;

    _outport_arrival[out_port - gC].push_back(GetSimTime());
}

void DGB::DecrementWaitingFlit(int out_port)
{
    assert((out_port >= gC) && (out_port < _outputs));
    _waiting_flit[out_port - gC] -= 1;
    assert(_waiting_flit[out_port - gC] >= 0);
}

void DGB::RegisterWaitingFlit()
{
    for (int iter = 0; iter < (_outputs - gC); iter++){
        _waiting_flit_dq[iter].push_back(_waiting_flit[iter]);

        // Remove old data
        while (_waiting_flit_dq[iter].size() > (unsigned)EPOCH_SIZE){
            _waiting_flit_dq[iter].pop_front();
        }
    }
}

float DGB::GetAvgWaitingFlit(int out_port)
{
    assert((out_port >= gC) && (out_port < _outputs));
    
    int sum = 0;
    unsigned int size = _waiting_flit_dq[out_port - gC].size();

    for (unsigned int iter = 0; iter < size; iter++){
        sum += _waiting_flit_dq[out_port - gC][iter];
    }

    if (size == 0)  return 0.0;
    else            return ((float)sum / (float)size);
}

float DGB::GetEstimatedContention(int out_port)
{
    float wait_time = GetAvgWaitingFlit(out_port);
    float arrival_rate = GetArrivalRate(out_port);
    float estimate;

    if (arrival_rate > 0)
        estimate = round((float)wait_time / (float)arrival_rate);
    else
        estimate = 0.0; 

    return estimate;
}

// USING LOCAL CONTENTION AS LOCAL INFORMATION
/*
void DGB::RegisterContention(int min_port, int min, int contention)
{
    assert((min_port >= 0) && (min_port < (_outputs - gC)));
    assert((min == 0) || (min == 1));

    _local_contention[min_port - gC][min].push_back(make_pair(GetSimTime(), contention));

    // To limit the buffer size
    while(_local_contention[min_port - gC][min].size() > N_HISTORY){
        _local_contention[min_port - gC][min].pop_front();
    }
}

float DGB::GetContention(int min_port, int min)
{
    assert((min_port >= gC) && (min_port < _outputs));
    assert((min == 0) || (min == 1));

    int sum = 0;
    //unsigned int count = 0;

    // Retire old data
    while ((_local_contention[min_port - gC][min].size() > 1) && ((_local_contention[min_port - gC][min].front().first + EPOCH_SIZE) < GetSimTime())){
    //while ((!_local_contention[min_port - gC][min].empty()) && ((_local_contention[min_port - gC][min].front().first + EPOCH_SIZE) < GetSimTime())){
		_local_contention[min_port - gC][min].pop_front();
	}

    unsigned int size = _local_contention[min_port - gC][min].size();
    for (unsigned int iter = 0; iter < size; iter++){
        sum += _local_contention[min_port - gC][min][iter].second;
    }

    if (size == 0){
        return 0;
    } else {
        return ((float)sum / (float)size);
    }
}
*/

void DGB::RegisterMinLocalLatency(int dest_router, int latency)
{
    // _min_local_latency[dest_router] += latency;
    // _n_min_local_latency[dest_router] += 1;

    assert(latency >= 0);

    _min_local_latency_deque[dest_router].push_back(make_pair(GetSimTime(), latency));

    // HANS: Cannot keep too many data
    while (_min_local_latency_deque[dest_router].size() > N_HISTORY){
        _min_local_latency_deque[dest_router].pop_front();
    }
}

void DGB::RegisterNonLocalLatency(int dest_router, int local_non_port, int latency)
{
    assert(latency >= 0);

    _non_local_latency[dest_router][local_non_port] += latency;
    _n_non_local_latency[dest_router][local_non_port] += 1;
}

float DGB::GetMinLocalLatency(int dest_router)
{
    int sum = 0;
    unsigned int count = 0;
    
    /*
    if(_n_min_local_latency[dest_router] == 0){

        return 0;

    } else {

        int sum = _min_local_latency[dest_router];
        int count = _n_min_local_latency[dest_router];
        assert(count > 0);

        return ((float)sum / (float)count);
    }
    */

    /*
    while ((!_min_local_latency_deque[dest_router].empty()) && (_min_local_latency_deque[dest_router].front().first + 1000 < GetSimTime())){
        _min_local_latency_deque[dest_router].pop_front();
    }
    */

    count = _min_local_latency_deque[dest_router].size();
    for (unsigned int iter = 0; iter < count; iter++){
        sum += _min_local_latency_deque[dest_router][iter].second;
    }

    if (count == 0){
        return 0.0;
    } else {
        return ((float)sum / (float)count);
    }
}

float DGB::GetNonLocalLatency(int dest_router, int local_non_port)
{
    if(_n_non_local_latency[dest_router][local_non_port] == 0){

        return 0;

    } else {

        int sum = _non_local_latency[dest_router][local_non_port];
        int count = _n_non_local_latency[dest_router][local_non_port];
        assert(count > 0);

        return ((float)sum / (float)count);
    }
}

float DGB::GetNonLocalLatencyAvg(int dest_router)
{
    int sum = 0;
    int count = 0;

    for (int iter = 0; iter < (_outputs - gC); iter++){
        sum += _non_local_latency[dest_router][iter];
        count += _n_non_local_latency[dest_router][iter];
    }

    if (count == 0)     return 0.0;
    else                return ((float)sum / (float)count);
}

/*
float DGB::GetAllLocalLatency(int dest_router){
    int sum = _local_latency[dest_router][0] + _local_latency[dest_router][1];
    int count = _n_local_latency[dest_router][0] + _n_local_latency[dest_router][1];

    if (count == 0){
        return 0;
    } else {
        assert(count > 0);
        return ((float)sum / (float)count);
    }
}
*/

void DGB::DeleteLocalLatency(int dest_router, int local_non_port)
{
    // HANS: Delete old info
    // _min_local_latency[dest_router] = 0;
    // _n_min_local_latency[dest_router] = 0;

    _non_local_latency[dest_router][local_non_port] = 0;
    _n_non_local_latency[dest_router][local_non_port] = 0;
}

/*
float DGB::GetLocalLatency(int dest_router, int min, int bias, unsigned int *count)
{
    assert((min == 0) || (min == 1));

    int sum = 0;

    // Retire old data
    //while ((_local_latency[dest_router][min].size() > 1) && ((_local_latency[dest_router][min].front().first + EPOCH_SIZE) < GetSimTime())){
    //while ((!_local_latency[dest_router][min].empty()) && ((_local_latency[dest_router][min].front().first + EPOCH_SIZE) < GetSimTime())){
	// 	_local_latency[dest_router][min].pop_front();
	//}

    *count = 0;
    unsigned int size = _local_latency[dest_router][min].size();
    for (unsigned int iter = 0; iter < size; iter++){
        sum += _local_latency[dest_router][min][iter].second;
        *count += 1;
    }
    
    if(*count == 0){
        return 0;
    } else {

        // HANS: Empty data from the current batch
        while (!_local_latency[dest_router][min].empty()){
            _local_latency[dest_router][min].pop_front();
        }

        assert(*count > 0);
        return ((float)sum / (float)*count);
    }
}
*/

void DGB::GetSizes(int dest_router, int local_non_port, unsigned int *min_count, unsigned int *non_count)
{
    /*
    while ((!_min_local_latency_deque[dest_router].empty()) && (_min_local_latency_deque[dest_router].front().first + 1000 < GetSimTime())){
        _min_local_latency_deque[dest_router].pop_front();
    }
    */
    *min_count = _min_local_latency_deque[dest_router].size();

    *non_count = _n_non_local_latency[dest_router][local_non_port];

    // *non_count_all = 0;
    // for (int iter = 0; iter < (_outputs - gC); iter++){
    //     *non_count_all += _n_non_local_latency[dest_router][iter];
    // }

}

// FOR ESTIMATING SERVICE RATE AT THE NEXT ROUTER
/*
void DGB::RegisterCredit(int out_port)
{
    assert((out_port >= gC) && (out_port < _outputs));
    
    _port_credit[out_port - gC].push(GetSimTime());
}

int DGB::GetServiceRate(int out_port)
{
    // Sanity checks
    assert((out_port >= gC) && (out_port < _outputs));

    while ((!_port_credit[out_port - gC].empty()) && ((_port_credit[out_port - gC].front() + WINDOW_SIZE) < GetSimTime())){
		_port_credit[out_port - gC].pop();
	}

    return _port_credit[out_port - gC].size();
}
*/

float DGB::GetBias(int local_min_port, int local_non_port, int global_min_port, int global_non_port, int h_min, int h_non, int q_min, int q_non, int q_min_noinflight, int q_non_noinflight, int dest_router, int chan_diff, int q_min_global, int q_non_global, int q_min_global_noinflight, int q_non_global_noinflight)
{
    //assert((min_port >= gC) && (min_port < _outputs));
    assert((dest_router >= 0) && (dest_router < _num_routers));

    // Train based on local information
    DoTraining(local_min_port, local_non_port, global_min_port, global_non_port, h_min, h_non, q_min, q_non, q_min_noinflight, q_non_noinflight, dest_router, chan_diff, q_min_global, q_non_global, q_min_global_noinflight, q_non_global_noinflight);

    // if (local_min_port == local_non_port)
    //     return GetBiasAvg(dest_router);
    // else 
    return (round(_bias_vect[dest_router][local_non_port]));

    // HANS: FOR PAPER - To compare the performance when there is only 1 bias for each source-destination pair
    //return (GetBiasAvg(dest_router));
}

float DGB::GetBiasNoTrain(int dest_router, int local_non_port)
{
    return (round(_bias_vect[dest_router][local_non_port]));
}

float DGB::GetBiasAvg(int dest_router){
    float sum = 0.0;

    for (int iter = 0; iter < _outputs - gC; iter++){
        //if (iter != 0) // Exclude the minimal port
            sum += _bias_vect[dest_router][iter];
    }

    float avg = (float)sum / (float)(_outputs - gC);

    return round(avg);
    //return avg;
}

bool DGB::GetExplore(int dest_router, int local_non_port){
    bool temp = _explore_vect[dest_router][local_non_port];
    _explore_vect[dest_router][local_non_port] = false;
    return temp;
}

bool DGB::GetExploreAvg(int dest_router){
    int count = 0;

    for (int iter = 0; iter < _outputs - gC; iter++){
        if ((_explore_vect[dest_router][iter]) && (iter != 0)){ // Exclude the minimal port
            _explore_vect[dest_router][iter] = false;
            count = count + 1;
        }
    }

    cout << "COUNT: " << count << endl;
    if (count >= (_outputs - gC) / 2)
        return true;
    else
        return false;
}

// FOR GLOBAL INFORMATION FEEDBACK
void DGB::RegisterDiffMin(int src_router, float latency)
{
    //_min_diff_vect[src_router] += latency;
    //_n_min_diff_vect[src_router] += 1;

    _min_diff_vect[src_router] = latency;
    _n_min_diff_vect[src_router] = 1;
    //_min_arrived_vect[src_router] += 1;
}

void DGB::RegisterDiffNon(int src_router, int intm_router, int non_port, int global_non_port, float latency)
{
    //assert((non_port >= 0) && (non_port < (_outputs - gC)));
    assert((global_non_port >= 0) && (global_non_port < gK));

    if (gIsDragonfly){
        _non_diff_vect[src_router][non_port][global_non_port] = latency;
        _n_non_diff_vect[src_router][non_port][global_non_port] = 1;

    } else {
        if (gN == 1){
            _non_diff_vect[src_router][non_port][0] = latency;
            _n_non_diff_vect[src_router][non_port][0] = 1;

        } else {
            _non_diff_vect[src_router][non_port][intm_router / gK] = latency;
            _n_non_diff_vect[src_router][non_port][intm_router / gK] = 1;

        }
    }
}

learnset* DGB::GenerateLearnset(int src_router)
{
    int size_min = _n_min_diff_vect[src_router];
    int size_non = 0;

    // CHECK IF WE HAVE ENOUGH DATA TO GENERATE LEARNSET
    if (gIsDragonfly){
        for (int iter = 0; iter < (_outputs - gC); iter++){
            for (int iter_global_port = 0; iter_global_port < gK; iter_global_port++){
                size_non += _n_non_diff_vect[src_router][iter][iter_global_port];
            }
        }
    } else {
        if (gN == 1){
            for (int iter = 0; iter < (_outputs - gC); iter++){
                size_non += _n_non_diff_vect[src_router][iter][0];
            }
        } else {
            for (int iter = 0; iter < (_outputs - gC); iter++){
                for (int iter_row = 0; iter_row < gK; iter_row++){
                    size_non += _n_non_diff_vect[src_router][iter][iter_row];
                }
            }
        }
    }

    if ((size_min + size_non) > 0){
        learnset* l = new learnset();
        l->gen_time = GetSimTime();
        l->src_router = _rID;

        // float sum_min = 0.0;
        // // Generate difference for MIN-routed packets
        // for (int iter = 0; iter < size_min; iter++){
        //     sum_min += _min_diff_vect[src_router][iter].second;
        // }

        if (size_min == 0){
            // Follow UGAL

            //1) Ratio
            //l->diff_min = 1;

            //2) Delta
            //l->diff_min.push(0.0);

            //3) Do nothing
        } else {
            //l->diff_min.push((float)sum_min / (float)size_min);
            l->diff_min.push((float)_min_diff_vect[src_router] / (float)size_min);

            //l->arrived_min = _min_arrived_vect[src_router];
            //_min_arrived_vect[src_router] = 0;
        }

        // Generate difference for NON-routed packets
        for (int iter = 0; iter < (_outputs - gC); iter++){

            float sum_non = 0.0;
            int size_non = 0;

            if (gIsDragonfly){
                for (int iter_global_port = 0; iter_global_port < gK; iter_global_port++){
                    sum_non += _non_diff_vect[src_router][iter][iter_global_port];
                    size_non += _n_non_diff_vect[src_router][iter][iter_global_port];
                }
            } else {
                if (gN == 1){
                    sum_non = _non_diff_vect[src_router][iter][0];
                    size_non = _n_non_diff_vect[src_router][iter][0];
                } else {
                    for (int iter_row = 0; iter_row < gK; iter_row++){
                        sum_non += _non_diff_vect[src_router][iter][iter_row];
                        size_non += _n_non_diff_vect[src_router][iter][iter_row];
                    }
                }
            }

            if (size_non > 0){
                float avg = (float)sum_non / (float)size_non;
                l->diff_non.push(make_pair(iter, avg));
            } else {
                if (size_non != 0)  cout << GetSimTime() << " - SizeNon: " << size_non << " | SrcRouter: " << src_router << " | Iter: " << iter << " | gN: " << gN << endl;

                assert(size_non == 0);

                // Is the MIN path sustainable?
                // if (!IsMinSustainable(src_router))
                //     l->diff_non.push(make_pair(iter, -1000.0)); // By using a large negative number, the estimated latency will be (contention + hop)
            }
        }

        // DELETE OLD DATA AFTER GENERATING LEARNSET
        _min_diff_vect[src_router] = 0.0;
        _n_min_diff_vect[src_router] = 0;

        // for (int iter = 0; iter < (_outputs - gC); iter++){
        //     _non_diff_vect[src_router][iter] = 0.0;
        //     _n_non_diff_vect[src_router][iter] = 0;
        // }

        if (gIsDragonfly){
            for (int iter = 0; iter < (_outputs - gC); iter++){
                for (int iter_global_port = 0; iter_global_port < gK; iter_global_port++){
                    _non_diff_vect[src_router][iter][iter_global_port] = 0.0;
                    _n_non_diff_vect[src_router][iter][iter_global_port] = 0;
                }
            }
        } else {
            if (gN == 1){
                for (int iter = 0; iter < (_outputs - gC); iter++){
                    _non_diff_vect[src_router][iter][0] = 0.0;
                    _n_non_diff_vect[src_router][iter][0] = 0;
                }
            } else {
                for (int iter = 0; iter < (_outputs - gC); iter++){
                    for (int iter_row = 0; iter_row < gK; iter_row++){
                        _non_diff_vect[src_router][iter][iter_row] = 0.0;
                        _n_non_diff_vect[src_router][iter][iter_row] = 0;
                    }
                }
            }
        }
        
        return l;

    } else {
        return NULL;
    }
}


void DGB::DoTraining(int local_min_port, int local_non_port, int global_min_port, int global_non_port, int h_min, int h_non, int q_min, int q_non, int q_min_noinflight, int q_non_noinflight, int dest_router, int chan_diff, int q_min_global, int q_non_global, int q_min_noinflight_global, int q_non_noinflight_global)
{
    assert((local_min_port >= 0) && (local_min_port < _outputs - gC));
    assert((local_non_port >= 0) && (local_non_port < _outputs - gC));

    if (gIsDragonfly){
        assert((global_min_port >= 0) && (global_min_port < 2 * gK * gK));
        assert((global_non_port >= 0) && (global_non_port < 2 * gK * gK));
    }

    // GET # OF MIN AND NON PACKETS
    unsigned int min_size, non_size;
    GetSizes(dest_router, local_non_port, &min_size, &non_size);

    // bool count_eligible = (min_size > 0) && (non_size > 0) && ((min_size + non_size) > 10);
    bool count_eligible = (min_size >= 1) && (non_size >= 1); // 2,2
    
    bool count_enough = ((min_size + non_size) >= 3);

    // HANS: Turn off dynamic epoch size
    //count_eligible = false;

   // Check if we should do training
    //if (((GetSimTime() - _last_localtrain_time[dest_router][local_non_port]) >= EPOCH_SIZE) || (count_eligible)){
    if (((GetSimTime() - _last_localtrain_time[dest_router][local_non_port]) >= EPOCH_SIZE) || (count_eligible) || (count_enough)){

        // Update time
        _last_localtrain_time[dest_router][local_non_port] = GetSimTime();

        // SHOULD WE TRAIN?
        bool train = true;
        if ((min_size == 0) || (non_size == 0)){
            train = false;
        }

        // CALCULATING HOW MUCH THE BIAS IS TO BE INCREMENTED DURING EXPLORATION (EXPLORE STEP)
        // ALSO, CALCULATE EXPLORATION PROBABILITY
        //unsigned int min_global_size;

        float explore_step;
        float explore_prob;

        //1) Dynamic
        float vacancy;

        if (gIsDragonfly){
            if (local_min_port >= (3 *gC - 1)){ // Global port
                assert(q_min == q_min_global);
                vacancy = _max_global_buff - q_min;

                explore_prob = (float)vacancy / (float)_max_global_buff;

                if (vacancy <= 0)   explore_step = 0.0;
                else                explore_step = explore_prob * EXPLORE_RATE * chan_diff;
                // else                explore_step = EXPLORE_RATE * chan_diff;


            } else { // Local port

#ifdef DGB_PIGGYBACK
                int global_q = GetGlobalQ(global_min_port);

                int local_vacancy = _max_local_buff - q_min;
                int global_vacancy = _max_global_buff - GetGlobalQ(global_min_port);
                vacancy = min(local_vacancy, global_vacancy);

                float local_explore_prob = (float)local_vacancy / (float)_max_local_buff;
                float global_explore_prob = (float)global_vacancy / (float)_max_global_buff;
                explore_prob = min(local_explore_prob, global_explore_prob);

                if (vacancy <= 0)   explore_step = 0.0;
                else                explore_step = explore_prob * EXPLORE_RATE * chan_diff;

#else
                assert(q_min_global >= 0);

                int local_vacancy = _max_local_buff - q_min;
                int global_vacancy = _max_global_buff - q_min_global;

                vacancy = min(local_vacancy, global_vacancy);

                float local_explore_prob = (float)local_vacancy / (float)_max_local_buff;
                float global_explore_prob = (float)global_vacancy / (float)_max_global_buff;
                explore_prob = min(local_explore_prob, global_explore_prob);

                if (vacancy <= 0)   explore_step = 0.0;
                else                explore_step = explore_prob * EXPLORE_RATE * chan_diff;
                // explore_step = EXPLORE_RATE * chan_diff;
#endif

            }

        } else { // 1D and 2D Flattened Butterfly
            vacancy = _max_global_buff - q_min;

            explore_prob = (float)vacancy / (float)_max_global_buff;
            
            if (vacancy <= 0)   explore_step = 0.0;
            //else                explore_step = (1.0 - ((float)non_size / (float)(vacancy))) * EXPLORE_RATE * chan_diff;
            else                explore_step = explore_prob * EXPLORE_RATE * chan_diff;
            //else                explore_step = EXPLORE_RATE * chan_diff;

        }

        //2) Fixed
        // explore_step = 1.0 - ((float)non_size / (float)q_min)
        // explore_prob = 0.05;

        if (explore_step < 0)   explore_step = 0.0;

        // HANS: Testing STATIC exploration step
        //explore_step = 10.0;

/*
#ifdef DGB_PIGGYBACK
        int globalq = GetGlobalQ(min_port);
        explore_prob = (float)(_max_global_buff - globalq) / (float)_max_global_buff;
        
#else 

        if (local_min_port >= (3 *gC - 1)){
            explore_prob = (float)(vacancy) / (float)_max_global_buff;
        } else {
            explore_prob = (float)(vacancy) / (float)_max_local_buff;
        }
#endif
*/

        assert(!isnan(explore_prob));

        bool explore = false;

        if (train) {
            _is_train[dest_router] = true;

            float min_avg = GetMinLocalLatency(dest_router);
            float non_avg = GetNonLocalLatency(dest_router, local_non_port);
            // if (non_size > 0){
            //     non_avg = GetNonLocalLatency(dest_router, local_non_port);
            // } else {
            //     assert(non_size_all > 0);
            //     non_avg = GetNonLocalLatencyAvg(dest_router);
            // }

            float step = LEARNING_RATE * (non_avg - min_avg);

            if ((step < explore_step) && (RandomFloat(1) < explore_prob)){
                //if ((non_size > 0) || (_bias_vect[dest_router][local_non_port] <= 0.0)) {
                explore = true;
                step = explore_step;
                _explore_vect[dest_router][local_non_port] = true;
                //} 
            }

            _bias_vect[dest_router][local_non_port] = _bias_vect[dest_router][local_non_port] + step;

            // Reset latency (testing new position)
            DeleteLocalLatency(dest_router, local_non_port);

            // FOR DEBUGGING
            // if ((_rID == 0) && (dest_router == 255) && (local_non_port == local_min_port))
            // if ((_rID == 0) && (dest_router == 255) && (local_non_port == 4))
            // if ((_rID == 0) && (dest_router == 255))
            // if (_rID == 0)
                //cout << GetSimTime() << " - BiasStep - rID: " << _rID << " | MinPort: " << local_min_port << " | NonPort: " << local_non_port << " | DestRouter: " << dest_router << " | Step: " << step << " | CurrBias: " << _bias_vect[dest_router][local_non_port] << " | MinAvg: " << min_avg << " | NonAvg: " << non_avg << " | MinSize: " << min_size << " | NonSize: " << non_size << " | ExploreProb: " << explore_prob << " | ExploreStep: " << explore_step << " | Explore: " << explore << " | QMin: " << q_min << " | QMinNoInflight: " << q_min_noinflight << endl;

        } else { // No MIN and NON information
            float random = RandomFloat(1);

            if (random < explore_prob){
                explore = true;

                // HANS: Bound exploration
                //if (_bias_vect[dest_router] < _max_exploration_bound[dest_router]){
                if ((non_size > 0) || (_bias_vect[dest_router][local_non_port] <= 0.0)){
                    _bias_vect[dest_router][local_non_port] = _bias_vect[dest_router][local_non_port] + explore_step;
                    _explore_vect[dest_router][local_non_port] = true;
                }

                // FOR DEBUGGING
                // if ((_rID == 0) && (dest_router == 255) && (local_non_port == local_min_port))
                // if (_rID == 0)
                    //cout << GetSimTime() << " - SpecialExplore - SrcRouter: " << _rID << " | DestRouter: " << dest_router << " | NonPort: " << local_non_port << " | MinSize: " << min_size << " | NonSize: " << non_size << " | QminNet: " << q_min_noinflight << " | CurrBias: " << _bias_vect[dest_router][local_non_port] << " | ExploreStep: " << explore_step << " | ExploreProb: " << explore_prob << endl;
            } else {
                float min_avg = 0.0;
                float non_avg = 0.0;
                float min_diff, non_diff;
                float emergency_step = 0.0;

                // ARE WE SURE THIS IS THE RIGHT THING TO DO?
                if (min_size > 0){ // Check if the estimated latency of the NON path is higher
                    assert(non_size == 0);

                    min_avg = GetMinLocalLatency(dest_router);
                    
                    unsigned int emergency_non_size;
                    non_diff = GetMyDiffNon(dest_router, local_non_port, &emergency_non_size);
                    if (emergency_non_size > 0){
#ifdef DGB_PIGGYBACK
                        if (gIsDragonfly){
                            int global_queue = GetGlobalQNet(global_non_port);
                            if (global_queue < 0)   global_queue = q_non_noinflight;

                            non_avg = q_non_noinflight + global_queue + non_diff + h_non;
                        } else {
                            non_avg = q_non_noinflight + non_diff + h_non;
                        }
#else
                        if (gIsDragonfly){
                            non_avg = q_non_noinflight + q_non_noinflight_global + non_diff + h_non;
                        } else {
                            non_avg = q_non_noinflight + non_diff + h_non;
                        }
#endif
                    } else {
#ifdef DGB_PIGGYBACK
                        if (gIsDragonfly){
                            int global_queue = GetGlobalQNet(global_non_port);
                            if (global_queue < 0)   global_queue = q_non_noinflight;

                            non_avg = q_non_noinflight + (h_non - 1) * global_queue;

                        } else {
                            non_avg = h_non * q_non_noinflight;
                        }
#else
                        if (gIsDragonfly){
                            non_avg = q_non_noinflight + (h_non - 1) * q_non_noinflight_global;
                            //non_avg = (h_non - 2) * q_non_noinflight + 2 * q_non_global;
                        } else {
                            non_avg = h_non * q_non_noinflight;
                        }
#endif
                    }

                    if (min_avg > non_avg)
                        emergency_step = LEARNING_RATE * (non_avg - min_avg);
                    
                } else if (non_size > 0) { // Check if the estimated latency of the MIN path is higher
                    assert(min_size == 0);

                    non_avg = GetNonLocalLatency(dest_router, local_non_port);

                    unsigned int emergency_min_size;
                    min_diff = GetMyDiffMin(dest_router, &emergency_min_size);
                    if (emergency_min_size > 0){
#ifdef DGB_PIGGYBACK
                        if (gIsDragonfly){
                            int global_queue = GetGlobalQNet(global_min_port);
                            if (global_queue < 0)   global_queue = q_min_noinflight;

                            min_avg = q_min_noinflight + global_queue + min_diff + h_min;

                        } else {
                            min_avg = q_min_noinflight + min_diff + h_min;
                        }
#else
                        if (gIsDragonfly){
                            min_avg = q_min_noinflight + q_min_noinflight_global + min_diff + h_min;
                        } else {
                            min_avg = q_min_noinflight + min_diff + h_min;
                        }
#endif
                    } else {
#ifdef DGB_PIGGYBACK
                        if (gIsDragonfly){
                            int global_queue = GetGlobalQNet(global_min_port);
                            if (global_queue < 0)   global_queue = q_min_noinflight;

                            min_avg = q_min_noinflight + (h_min - 1) * global_queue;
                        } else {
                            min_avg = h_min * q_min_noinflight;
                        }
#else
                        if (gIsDragonfly){
                            min_avg = q_min_noinflight + (h_min - 1) * q_min_noinflight_global;
                            //min_avg = (h_min - 1) * q_min_noinflight + q_min_global;
                        } else {
                            min_avg = h_min * q_min_noinflight;
                        }
#endif
                    }

                    if (non_avg > min_avg)
                        emergency_step = LEARNING_RATE * (non_avg - min_avg);

                } else {
                    // FOR DEBUGGING
                    //if ((_rID == 143) && (dest_router == 149) && (local_non_port == 7))
                    // if ((_rID == 0) && (dest_router == 255) && (local_non_port == local_min_port))
                    // if (_rID == 0)
                    //     cout << GetSimTime() << " - NoExploration - ExploreProb: " << explore_prob << " | CurrBias: " << _bias_vect[dest_router][local_non_port] << " | LocalNonPort: " << local_non_port << " | MinSize: " << min_size << " | NonSize: " << non_size << " | Hmin: " << h_min << " | Qmin: " << q_min << " | Hnon: " << h_non << " | Qnon: " << q_non << endl;
                }

                _bias_vect[dest_router][local_non_port] = _bias_vect[dest_router][local_non_port] + emergency_step;

                // if ((_rID == 8) && (dest_router == 247) && (local_non_port == local_min_port))
                    //cout << GetSimTime() << " - EMERGENCY_STEP: " << emergency_step << " | CurrBias: " << _bias_vect[dest_router][local_non_port] << " | MinSize: " << min_size << " | NonSize: " << non_size << " | MinAvg: " << min_avg << " | NonAvg: " << non_avg << " | Hnon: " << h_non << " | QnonNet: " << q_non_noinflight << " | MinPort: " << local_min_port << " | NonPort: " << local_non_port << endl;
            }
        }

        // Reset latency (default position)
        //DeleteLocalLatency(dest_router);

        // Reset exploration bound
        //_max_exploration_bound[dest_router] = 0.0;

        // HANS: Bound bias value
        float min_bias_value, max_bias_value;

        max_bias_value = _max_global_buff * h_min / h_non;
        min_bias_value = -1 * _max_global_buff;

        if (_bias_vect[dest_router][local_non_port] < min_bias_value)      _bias_vect[dest_router][local_non_port] = min_bias_value;
        if (_bias_vect[dest_router][local_non_port] > max_bias_value)      _bias_vect[dest_router][local_non_port] = max_bias_value;

        // FOR PAPER
        // if ((_rID == 0) && (dest_router == 1) && (local_non_port == 1)){
        //     if (explore)
        //         cout << GetSimTime() << "\t" << round(_bias_vect[dest_router][local_non_port]) << "\t" << round(_bias_vect[dest_router][local_non_port]) << endl;
        //     else 
        //         cout << GetSimTime() << "\t" << round(_bias_vect[dest_router][local_non_port]) << endl; 
        // }
   }
}


void DGB::ReceiveLearnset(learnset* l)
{
    assert(l != NULL);

    if (l->gen_time >= _latest_learnset_gen_time[l->src_router]){ //HANS: Only use the latest learnset
        while (!l->diff_min.empty()){
            // HANS: There should only be one value
            assert(l->diff_min.size() == 1);

            _my_min_diff_vect[l->src_router].push_back(make_pair(GetSimTime(), l->diff_min.front()));

            while (_my_min_diff_vect[l->src_router].size() > 1){ // HANS: Only keep the latest info, any better option?
                _my_min_diff_vect[l->src_router].pop_front();
            }

            l->diff_min.pop();

            // FOR DEBUGGING: Stucked MIN packets
            // if (_rID == 0){
            //     _min_departure_counter[l->src_router] -= l->arrived_min;
            //     cout << "Stucked: " << _min_departure_counter[l->src_router] << endl;
            // }

        }

        while (!l->diff_non.empty()){
            pair<int, float> temp = l->diff_non.front();

            _my_non_diff_vect[l->src_router][temp.first].push_back(make_pair(GetSimTime(), temp.second));

            while (_my_non_diff_vect[l->src_router][temp.first].size() > 1){ // HANS: Only keep the latest info, any better option?
                _my_non_diff_vect[l->src_router][temp.first].pop_front();
            }

            l->diff_non.pop();
        }

        // Update latest learnset generate time
        _latest_learnset_gen_time[l->src_router] = l->gen_time;

        // HANS: For debugging
        //if (_rID == 0)
            //cout << GetSimTime() << " - RcvLS - lID: " << l->id << " | rID: " << _rID << " | Generated by router: " << l->src_router << endl;
    }

    delete l;
}

float DGB::GetMyDiffMin(int dest_router, unsigned int* count)
{
    float sum = 0.0;
    *count = _my_min_diff_vect[dest_router].size();

    for (unsigned int iter = 0; iter < *count; iter++){
        sum += _my_min_diff_vect[dest_router][iter].second;
    }

    if (*count == 0){
        // HANS: CAUTION -> if there is no data, assumed that the data from UGAL is right
        
        //1) Ratio
        //return 1;

        //2) Delta
        return 0.0;
    } else {
        float avg = (float)sum / (float)*count;

        // HANS: For debugging
        // if ((_rID == 0) && (dest_router == 255)){
        //     cout << GetSimTime() << " - GetMyLatMin - Avg: " << avg << endl; 
        // }

        return avg;
    }
}

float DGB::GetMyDiffNon(int dest_router, int non_port, unsigned int* count)
{
    assert((non_port >= 0) && (non_port < (_outputs - gC)));

    float sum = 0.0;
    *count = _my_non_diff_vect[dest_router][non_port].size();

    for (unsigned int iter = 0; iter < *count; iter++){
        sum += _my_non_diff_vect[dest_router][non_port][iter].second;
    }

    if (*count == 0){
        // HANS: CAUTION -> if there is no data, assumed that the data from UGAL is right

        //1) Ratio
        //return 1;

        //2) Delta (Decoupling)
#ifdef DECOUPLING
        return 0;

        //3) Average (NO decoupling)
#else
        float avg = GetMyDiffNonAvg(dest_router);
        return avg;
#endif

    } else {
        float avg = (float)sum / (float)*count;
        return avg;
    }
}

float DGB::GetMyDiffNonAvg(int dest_router)
{
    float sum = 0.0;
    unsigned int total_size = 0;

    for (int iter_port = 0; iter_port < (_outputs - gC); iter_port++){
        unsigned int size = _my_non_diff_vect[dest_router][iter_port].size();
        assert(size <= 1); //HANS: Only keep the latest info
        total_size += size;

        for (unsigned int iter_dq = 0; iter_dq < size; iter_dq++){
            sum += _my_non_diff_vect[dest_router][iter_port][iter_dq].second;
        }
    }

    if (total_size == 0){
        return 0;
    } else {
        assert(total_size > 0);         

        float avg = (float)sum / (float)total_size;
        return avg;
    }
}

#ifdef DGB_PIGGYBACK
void DGB::RegisterGlobalQ(int global_port_idx, int global_queue)
{
    // HANS: Sanity check
    assert((global_port_idx >= 0) && (global_port_idx < (2 * gK * gK)));

    _globalq_vect[global_port_idx] = global_queue;
}

int DGB::GetGlobalQ(int global_port_idx)
{
    assert((global_port_idx >= 0) && (global_port_idx < (2 * gK * gK)));
    return _globalq_vect[global_port_idx];
}

void DGB::RegisterGlobalQNet(int global_port_idx, int global_queue_net)
{
    // HANS: Sanity check
    assert((global_port_idx >= 0) && (global_port_idx < (2 * gK * gK)));

    _globalqnet_vect[global_port_idx] = global_queue_net;
}

int DGB::GetGlobalQNet(int global_port_idx)
{
    assert((global_port_idx >= 0) && (global_port_idx < (2 * gK * gK)));
    return _globalqnet_vect[global_port_idx];
}

#endif

void DGB::ResetBias(int dest_router)
{
    for (int iter_port = 0; iter_port < (_outputs - gC); iter_port++){
        _bias_vect[dest_router][iter_port] = 0.0;
    }
}

void DGB::ResetAllBias()
{
    for (int iter_router = 0; iter_router < _num_routers; iter_router++){
        for (int iter_port = 0; iter_port < (_outputs - gC); iter_port++)
            _bias_vect[iter_router][iter_port] = 0.0;
    }
}

/*
void DGB::IncrementDepartureCounter(int dest_router, int non_port, int min)
{
    if (min == 1)
        _min_departure_counter[dest_router] += 1;
    else 
        _non_departure_counter[dest_router][non_port] += 1;
}
*/

#endif
