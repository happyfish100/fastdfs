i=0
while [ $i -lt 10 ]; do
  ./test_delete $i &
  let i=i+1
done

