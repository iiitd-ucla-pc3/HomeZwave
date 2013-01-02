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

BASE_PATH=str("/root/data/")+str(now.day)+"_"+str(now.month)+str("/")

if not os.path.exists(BASE_PATH):
	os.makedirs(BASE_PATH)

FILENAME=BASE_PATH+str(now.hour)+".csv"

print BASE_PATH
print FILENAME
#Connecting Database and Create CSV
db = MySQLdb.connect(host="localhost", user="root", passwd="password", db="home")
dump_writer = csv.writer(open(FILENAME,'w'), delimiter=',',quotechar="'")
cursor = db.cursor()
cursor.execute("SELECT * FROM smartswitch where time >= %s",a)
result = cursor.fetchall()
for record in result:
        dump_writer.writerow(record)
db.close()
