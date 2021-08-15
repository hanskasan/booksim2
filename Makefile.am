# -*- Makefile -*-
#
#

AM_CPPFLAGS = \
	$(MPI_CPPFLAGS) -D SINGLE_CYCLE -D DGB_ON

compdir = $(pkglibdir)
comp_LTLIBRARIES = libbooksim2.la

libbooksim2_la_SOURCES = \
	booksim2.cc \
	booksimBridge.cc \
	booksimInterface.cc \
	src/allocators/allocator.cpp \
  	src/allocators/islip.cpp \
  	src/allocators/loa.cpp \
  	src/allocators/maxsize.cpp \
  	src/allocators/pim.cpp \
  	src/allocators/selalloc.cpp \
  	src/allocators/separable_input_first.cpp \
  	src/allocators/separable.cpp \
  	src/allocators/wavefront.cpp \
  	src/arbiters/arbiter.cpp \
    src/arbiters/matrix_arb.cpp \
    src/arbiters/prio_arb.cpp \
    src/arbiters/roundrobin_arb.cpp \
    src/arbiters/tree_arb.cpp \
	src/learning/dgb.cpp \
  	src/networks/anynet.cpp \
    src/networks/cmesh.cpp \
    src/networks/dragonfly.cpp \
    src/networks/fattree.cpp \
    src/networks/flatfly_onchip.cpp \
    src/networks/fly.cpp \
    src/networks/kncube.cpp \
    src/networks/network.cpp \
    src/networks/qtree.cpp \
    src/networks/tree4.cpp \
  	src/power/buffer_monitor.cpp \
    src/power/power_module.cpp \
    src/power/switch_monitor.cpp \
  	src/routers/chaos_router.cpp \
    src/routers/event_router.cpp \
    src/routers/iq_router.cpp \
    src/routers/router.cpp \
  	src/batchtrafficmanager.cpp \
  	src/booksim_config.cpp \
    src/buffer_state.cpp \
  	src/buffer.cpp \
  	src/config_utils.cpp \
  	src/credit.cpp \
  	src/flit.cpp \
  	src/flitchannel.cpp \
  	src/injection.cpp \
  	src/misc_utils.cpp \
  	src/module.cpp \
  	src/outputset.cpp \
  	src/packet_reply_info.cpp \
  	src/random_utils.cpp \
	src/rng_double_wrapper.cpp \
	src/rng_wrapper.cpp \
  	src/routefunc.cpp \
  	src/stats.cpp \
  	src/traffic.cpp \
  	src/trafficmanager.cpp \
  	src/vc.cpp \
	booksim2.h \
	booksimBridge.h \
	booksimInterface.h \
	src/allocators/allocator.hpp \
  	src/allocators/islip.hpp \
  	src/allocators/loa.hpp \
  	src/allocators/maxsize.hpp \
  	src/allocators/pim.hpp \
  	src/allocators/selalloc.hpp \
  	src/allocators/separable_input_first.hpp \
  	src/allocators/separable.hpp \
  	src/allocators/wavefront.hpp \
  	src/arbiters/arbiter.hpp \
    src/arbiters/matrix_arb.hpp \
    src/arbiters/prio_arb.hpp \
    src/arbiters/roundrobin_arb.hpp \
    src/arbiters/tree_arb.hpp \
	src/learning/dgb.hpp \
  	src/networks/anynet.hpp \
    src/networks/cmesh.hpp \
    src/networks/dragonfly.hpp \
    src/networks/fattree.hpp \
    src/networks/flatfly_onchip.hpp \
    src/networks/fly.hpp \
    src/networks/kncube.hpp \
    src/networks/network.hpp \
    src/networks/qtree.hpp \
    src/networks/tree4.hpp \
  	src/power/buffer_monitor.hpp \
    src/power/power_module.hpp \
    src/power/switch_monitor.hpp \
  	src/routers/chaos_router.hpp \
    src/routers/event_router.hpp \
    src/routers/iq_router.hpp \
    src/routers/router.hpp \
  	src/batchtrafficmanager.hpp \
  	src/booksim_config.hpp \
  	src/booksim.hpp \
    src/buffer_state.hpp \
  	src/buffer.hpp \
  	src/channel.hpp \
  	src/config_utils.hpp \
  	src/credit.hpp \
  	src/flit.hpp \
  	src/flitchannel.hpp \
  	src/globals.hpp \
  	src/injection.hpp \
  	src/misc_utils.hpp \
  	src/module.hpp \
  	src/outputset.hpp \
  	src/packet_reply_info.hpp \
  	src/pipefifo.hpp \
  	src/random_utils.hpp \
  	src/routefunc.hpp \
  	src/stats.hpp \
  	src/timed_module.hpp \
  	src/traffic.hpp \
  	src/trafficmanager.hpp \
  	src/vc.hpp 

libbooksim2_la_LDFLAGS = -module -avoid-version