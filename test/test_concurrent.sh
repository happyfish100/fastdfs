i=0
while [ $i -lt 10 ]; do
  ./test_concurrent $i &
  let i=i+1
done

