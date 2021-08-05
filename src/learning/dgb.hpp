// Hans Kasan
// CSNL-KAIST
// DGB for routing

#ifdef DGB_ON

#include <cstdlib>
#include <cmath>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "flit.hpp"
#include "globals.hpp"
#include "random_utils.hpp"

// MODES
// 1) Choose how the feedback is sent to the source router
#define DGB_IDEAL
//#define DGB_PIGGYBACK

// 3) Select if the inflight packets should be removed
#define REMOVE_INFLIGHT

// 4) Choose T_est schemes
//#define T_EST_HQ_LOCAL
//#define T_EST_HQW_GLOBAL
#define T_EST_END2END
//#define T_EST_FROM2ND

// 5) Use decoupling?
#define DECOUPLING

using namespace std;

class DGB {
public:
    DGB(int rID, int outputs, int vcs, int max_local_buff, int max_global_buff);
    virtual ~DGB();

    // USING LITTLE'S LAW AS LOCAL INFORMATION
    vector<deque<int> > _outport_arrival;
    vector<int> _waiting_flit;
    vector<deque<int> > _waiting_flit_dq;

    //void RegisterService(int out_port, int time);
    float GetArrivalRate(int out_port);

    void IncrementWaitingFlit(int out_port);
    void DecrementWaitingFlit(int out_port); 
    void RegisterWaitingFlit();
    float GetAvgWaitingFlit(int out_port);
    float GetEstimatedContention(int out_port);

    // USING LATENCY AT THE HEAD OF THE INPUT BUFFER AS LOCAL INFORMATION
    // Sum of latency
    // vector<int>             _min_local_latency;
    vector<deque<pair<int, int> > >     _min_local_latency_deque;
    vector<vector<int> > _non_local_latency;

    // Number of local latency data
    //vector<int>             _n_min_local_latency;
    vector<vector<int> >    _n_non_local_latency;

    vector<vector<deque<pair<int, int> > > > _local_contention;

    //void RegisterContention(int min_port, int min, int contention);
    //float GetContention(int min_port, int min);

    void RegisterMinLocalLatency(int dest_router, int latency);
    void RegisterNonLocalLatency(int dest_router, int local_non_port, int latency);

    float GetMinLocalLatency(int dest_router);
    float GetNonLocalLatency(int dest_router, int local_non_port);
    float GetNonLocalLatencyAvg(int dest_router);
    //float GetAllLocalLatency(int dest_router);
    void DeleteLocalLatency(int dest_router, int local_non_port);
    void GetSizes(int dest_router, int local_non_port, unsigned int *min_count, unsigned int *non_count);

    // FOR ASSIGNING VALUE FOR EACH BIAS
    vector<vector<float> > _bias_weight;

    float GetBias(int local_min_port, int local_non_port, int global_min_port, int global_non_port, int h_min, int h_non, int q_min, int q_non, int q_min_noinflight, int q_non_noinflight, int dest_router, int chan_diff, int q_min_global, int q_non_global, int q_min_global_noinflight, int q_non_global_noinflight);
    float GetBiasNoTrain(int dest_router, int local_non_port);
    float GetBiasAvg(int dest_router);
    bool  GetExplore(int dest_router, int local_non_port);
    bool  GetExploreAvg(int dest_router);

    // FOR GLOBAL INFORMATION FEEDBACK
    //vector<deque<pair<int, float> > > _min_diff_vect; // Feedback to be sent to other routers
    //vector<vector<deque<pair<int, float> > > > _non_diff_vect; // Feedback to be sent to other routers
    vector<float>           _min_diff_vect; // Feedback to be sent to other routers
    vector<vector<vector<float> > >  _non_diff_vect;
    vector<int>             _n_min_diff_vect;
    vector<vector<vector<int> > >    _n_non_diff_vect;
    //vector<int>             _min_arrived_vect;
    //vector<vector<vector<int> > >    _non_arrived_vect;

    vector<deque<pair<int, float> > > _my_min_diff_vect; // Feedback received from other routers
    vector<vector<deque<pair<int, float> > > > _my_non_diff_vect; // Feedback received from other routers

    void RegisterDiffMin(int src_router, float latency);
    void RegisterDiffNon(int src_router, int intm_router, int non_port, int global_non_port, float latency);
    float  GetMyDiffMin(int dest_router, unsigned int *count);
    float  GetMyDiffNon(int dest_router, int non_port, unsigned int *count);
    float  GetMyDiffNonAvg(int dest_router);

    // DEPARTURE COUNTERS
    vector<int>          _min_departure_counter;
    vector<vector<int> > _non_departure_counter;
    //void IncrementDepartureCounter(int dest_router, int non_port, int min);

#ifdef DGB_PIGGYBACK
    // FOR GLOBAL QUEUE FEEDBACK
    vector<int> _globalq_vect;
    vector<int> _globalqnet_vect;

    void RegisterGlobalQ(int global_port_idx, int global_queue);
    int GetGlobalQ(int global_port_idx);

    void RegisterGlobalQNet(int global_port_idx, int global_queue_net);
    int  GetGlobalQNet(int global_port_idx);
#endif

    // FOR TRAINING (LOCAL AND GLOBAL)
    vector<vector<float> > _bias_vect;
    vector<vector<bool> > _explore_vect; // HANS: FOR PAPER
    vector<vector<int> > _last_localtrain_time;
    vector<bool> _is_train;
    
    //vector<float> _prev_local_cost;
    vector<float> _prev_local_lat;
    vector<int>   _prev_local_lat_count;
    vector<int> _latest_learnset_gen_time;

    vector<float> _max_exploration_bound;

    void DoTraining (int local_min_port, int local_non_port, int global_min_port, int global_non_port, int h_min, int h_non, int q_min, int q_non, int q_min_noinflight, int q_non_noinflight, int dest_router, int chan_diff, int q_min_global, int q_non_global, int q_min_global_noinflight, int q_non_global_noinflight);

    learnset* GenerateLearnset  (int src_router);
    void ReceiveLearnset        (learnset* l);

    // RESETTING BIAS VALUES
    //vector<float> _prev_all_avg;
    //vector<bool> _prev_all_avg_initialized;

    void ResetBias(int dest_router);
    void ResetAllBias();

    // REGISTER DEPARTURE TIME
    queue<int> _min_depart;
    vector<queue<int> > _non_depart;
    void RegisterDeparture(int min, int dest_router, int non_port);

private:
    int _rID;
    int _outputs;
    int _vcs;
    int _num_routers;

    int _max_local_buff;
    int _max_global_buff;

    // HYPERPARAMETERS
    int EPOCH_SIZE;

    static const int N_HISTORY = 5; // To limit the size of the history buffer (considering hardware cost)

    static const int MAX_BIAS  = 16;
    static const int MIN_BIAS  = -1 * MAX_BIAS;
    static const int BIAS_INC  = 8;
    static const int N_ACTIONS = (2 * (MAX_BIAS / BIAS_INC)) + 1 + 2;

    const float INFINITE = 10000.0; // Constant to represent infinite value

    float EXPLORE_RATE;
    float LEARNING_RATE;

    // FOR ESTIMATING UTILIZATION FROM BIAS
    vector<vector<vector<queue<int> > > > _estm_vect;
};

#endif
