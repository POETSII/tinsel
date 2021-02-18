

EXISTING = asp-gals asp-sync asp-tiles-sync \
	clocktree-async \
	hashmin-sync heat-cube-sync heat-gals heat-grid-sync heat-sync \
	izhikevich-gals izhikevich-sync \
	nhood-sync \
	pagerank-gals pagerank-sync

CPPFLAGS += -g -pthread -O3 -DNDEBUG=1

define setup_existing

build/existing/$1/%.o : ../$1/%.cpp $(wildcard include/*) $(wildcard include/*/*)
	mkdir -p build/existing/$1/
	echo "Wibble"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I include -I ../../../include -c $$< -o $$@

build/existing/$1/Run : build/existing/$1/Run.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS)  $$^ -o $$@ $(LDFLAGS) $(LDLIBS)

all_existing_o : build/existing/$1/Run.o
all_existing_exe : build/existing/$1/Run

endef

$(foreach x,$(EXISTING),$(eval $(call setup_existing,$(x))))

build/existing/heat-grid-sync/Run : build/existing/heat-grid-sync/Colours.o