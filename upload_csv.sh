current=0
while true
do
	hour=$(date +"%_H")
	previous=$current
	current=$hour
	if [ $current != $previous ]
		then 

			python create_csv.py
			sleep 10
			python dropbox_upload.py
			sleep 10
		
	fi
done
