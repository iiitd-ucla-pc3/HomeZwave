#!/bin/bash
while true
do
	python create_csv.py&
	sleep 10
	python dropbox_upload.py&
	sleep 3570
done
