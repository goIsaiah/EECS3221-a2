production:
	cc New_Alarm_Mutex.c -pthread

debug:
	cc New_Alarm_Mutex.c -DDEBUG -g -pthread

