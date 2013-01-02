from dbupload import DropboxConnection
from getpass import getpass
from pytz import timezone

import db_password
import random
import time
import glob
import os
import datetime
import MySQLdb
import csv
import time

email=db_password.email
password=db_password.password

now=datetime.datetime.now(timezone('Asia/Kolkata'))

BASE_UPLOAD_PATH="/SmartMeter/GroundTruth/Amarjeet/"
DATA_BASE_PATH="/root/data/"
BASE_PATH=str(now.day)+"_"+str(now.month)+str("/")
FILENAME=str(now.hour)+".csv"

print BASE_PATH
print FILENAME

#Path in Dropbox in which to upload the data
base_upload_path = BASE_UPLOAD_PATH + BASE_PATH
folders=os.listdir(DATA_BASE_PATH)

try:
    # Create the connection
    conn = DropboxConnection(email, password)
    for folder in folders:
        #List of files
        list_of_files=glob.glob(str(DATA_BASE_PATH)+str(folder)+str("/*.csv"))
        print list_of_files
        for FILENAME in list_of_files:
					#Upload the File
					print FILENAME +" will be uploaded"
					conn.upload_file(FILENAME,base_upload_path,FILENAME)
					os.remove(FILENAME)
except:
    print("Upload failed")
else:
    pass
