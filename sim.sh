# make clean
bash -c "cd rtl && make sim"
make -C hostlink
make -C apps/hello sim all
bash -c "cd rtl && ./sim-two.sh" & bash -c "cd apps/hello && sleep 5 && echo starting app && ./sim"
