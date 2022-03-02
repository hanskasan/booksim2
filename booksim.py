# Import the SST module
import sst

### Create the components
net_sim = sst.Component("booksim2", "booksim2")

### Parameterize the components.
# Run 'sst-info booksim2' at the command line 
# to see parameter documentation
params = {
        "booksim_clock" : "1GHz",
        "num_motif_nodes" : 0,
        "topology" : "dragonfly",    
        "routing_function" : "min_adapt",
        "packet_size" : 1,
        "num_vcs" : 2

}

net_sim.addParams(params)