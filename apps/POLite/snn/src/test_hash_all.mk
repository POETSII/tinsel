
PRACTRAND ?= ../../../../../external/stats/practrand/bin/RNG_test

define expand_test_pair_hash
test/test_pair_hash/$1_$2_practrand.txt : bin/test_pair_hash	
	mkdir -p test/test_pair_hash/
	bin/test_pair_hash $1 32 $2 | $(PRACTRAND) stdin32 -tlmin 1M -tlmax 4G -a > $$@.tmp
	mv $$@.tmp $$@

all_test_pair_hash : test/test_pair_hash/$1_$2_practrand.txt 

endef

KS := 16 17 18 19 20 21 22 23 24 25 26
MS := linear1 linear2 mod

$(foreach k,$(KS), $(foreach m,$(MS), $(eval $(call expand_test_pair_hash,$(k),$(m)))))