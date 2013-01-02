current=0
while(1)
{
	hour=$(date +"%_H")
	echo "Current Hour : $hour"
	previous=$current
	current=$hour
	if [ $current != $previous ]
		then python dropbox_upload.py
	fi

}
