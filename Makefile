debug:
	cc New_Alarm_Mutex.c -DDEBUG -g -pthread

production:
	cc New_Alarm_Mutex.c -pthread

