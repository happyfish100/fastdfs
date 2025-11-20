i=0
while [ $i -lt 10 ]; do
  ./test_range_download $i &
  let i=i+1
done

