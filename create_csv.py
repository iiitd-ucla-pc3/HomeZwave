import random
import time
import glob
import os
import datetime
import MySQLdb
import csv
import time
import os

from pytz import timezone

a=((int)(time.time()))-3600
now=datetime.datetime.now(timezone('Asia/Kolkata'))

SQL_DATABASE_LIMIT=str(5000)




#Connecting Database and Create CSV
db = MySQLdb.connect(host="localhost", user="root", passwd="password", db="home")

cursor = db.cursor()
cursor.execute("SELECT * FROM smartswitch ORDER BY time ASC LIMIT 1;")
first_row=cursor.fetchone()
cursor = db.cursor()
cursor.execute("SELECT * FROM smartswitch ORDER BY time ASC LIMIT "+SQL_DATABASE_LIMIT+";")
result = cursor.fetchall()
if first_row is not None:
	print "Start time ",first_row[1]
	start_time=datetime.datetime.fromtimestamp(first_row[1],timezone('Asia/Kolkata'))
	print start_time
	BASE_PATH=str("/root/data/")+str(start_time.day)+"_"+str(start_time.month)+str("/")
	FILENAME=BASE_PATH+str(start_time.hour)+str("_")+str(start_time.minute)+".csv"
	if not os.path.exists(BASE_PATH):
		os.makedirs(BASE_PATH)
	
	print BASE_PATH
	print FILENAME
	dump_writer = csv.writer(open(FILENAME,'w'), delimiter=',',quotechar="'")
	for record in result:
			print record
			dump_writer.writerow(record)
			
	cursor=db.cursor()		
	cursor.execute("DELETE FROM smartswitch ORDER BY time ASC LIMIT "+SQL_DATABASE_LIMIT+";")
	db.commit()

db.close()

