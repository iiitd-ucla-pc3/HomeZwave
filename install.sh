#!/bin/bash
#Install Dependencies
echo "Installing Dependencies"
apt-get install python-pip
pip install python-pip
pip install mechanize
pip install pytz
pip install mysql-python
apt-get install make
apt-get install g++
apt-get install libmysqlcppconn-dev
apt-get install libboost-dev
#Install MYSQL Server
mysql_pass=password
export DEBIAN_FRONTEND=noninteractive 
echo mysql-server mysql-server/root_password select $mysql_pass  | debconf-set-selections
echo mysql-server mysql-server/root_password_again select $mysql_pass | debconf-set-selections
echo "Installing Database"
apt-get -y install mysql-server
sleep 10
#Restart
service mysql restart
echo "MySQL Installation and Configuration is Complete."
apt-get install libmysqlcppconn-dev
RESULT=`mysql -u root -ppassword --skip-column-names -e "SHOW DATABASES LIKE 'home'"`
if [ "$RESULT"=="home" ]; then
    echo "Database exist"
else
    mysql -u root -ppassword -e "CREATE DATABASE home; use home; create table smartswitch(node int, time int, power int, primary key(node,time));"
	echo "Database Created"
fi
