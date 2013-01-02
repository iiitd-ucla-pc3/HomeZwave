current=0
while true
do
	hour=$(date +"%_H")
	min=$(date +"%M")
	previous=$current
	current=$hour
	if [ "$current" != "$previous" -a "$min" == "0" ]
		then 
			python create_csv.py&
			sleep 10
			python dropbox_upload.py&
			sleep 3570
	fi
done
