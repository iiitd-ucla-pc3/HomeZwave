current=0
hour=$(date +"%_H")
echo "Current Hour : $hour"
previous=$current
current=1
if [ $current != $previous ]
	then echo "Hello"
fi
