# First, run in another tab.
# export MallocStackLogging=1
# ./build/main


pid=$(ps aux | grep "./build/main" | head -n 1 | awk '{ print $2 }')

sudo leaks $pid
