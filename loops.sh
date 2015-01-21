# server loop

while (true)
do

# build and push
make push

# notify executable push
echo "[LOOP] notify push"
nc -z localhost 9999

# await program termination
echo "[LOOP] await term"
nc -l 9998

# sleep for a bit, to avoid race condition :)
sleep 2

# repeat
done
