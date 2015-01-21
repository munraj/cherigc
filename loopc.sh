# client loop

while (true)
do

# await executable push
echo "[LOOP] await push"
nc -l 9999

# run program
./gctest

# notify program termination
echo "[LOOP] notify term"
nc -z localhost 9998

# repeat
done
