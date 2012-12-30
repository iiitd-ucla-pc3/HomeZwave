RESULT=`mysql -u root -ppassword --skip-column-names -e "SHOW DATABASES LIKE 'home'"`
if [ "$RESULT"=="home" ]; then 
	echo "Database exist"
else
	echo "Hello"
fi
