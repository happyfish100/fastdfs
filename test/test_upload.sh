i=0
while [ $i -lt 10 ]; do
  ./test_upload $i &
  let i=i+1
done

