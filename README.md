# log_daemon

A simple log hub which manage host logs.

# build & run

```
mkdir -p build
cd build
cmake .. && make

# start daemon
./logd

# start a trivial publisher (producer)
./logtest

# subscribe message (consumer)
./logcat
```
