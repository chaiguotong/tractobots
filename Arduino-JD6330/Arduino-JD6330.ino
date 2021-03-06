/*
	JD6330 

  	transmission (shuttle)
		linear actuator
		PID
		motor driver
		neutral sense input

	steering
		proportional hydraulic valves
		PID
		2xMOSFET

	throttle
		analog?  PWM?
		PID with RPM or ground speed?

	3-pt.
		up
		down
		direct? MOSFET?

	engine enable
		MOSFET

	starter
		MOSFET

	lights
		?xMOSFET

	horn
		MOSFET
	
	PTO
		MOSFET
		3-wire?

	RPM?


	Arduino (8 digital out, 6 PWM out, 8 analog in)

	input
		steer position
		shuttle actuator position
		shuttle neutral sensor
		steering wheel override
		shuttle selector position
		seat switch?
		RPM?

	dual motor driver.0
		shuttle fore (PWM)
		shuttle aft (PWM)
		powershift fore (PWM)
		powershift aft (PWM)
	
	MOSFET4.0
		steer left (PWM)
		steer right (PWM)
		3-pt down
		3-pt up

	MOSFET4.1
		horn
		running lights
		work lights

	MOSFET4.2
		engine enable
		starter
		PTO



 */

#if (ARDUINO >= 100)
	#include <Arduino.h>
#else
	#include <WProgram.h>
#endif

#include <PID_v1.h>

#include <ros.h>
// #include <std_msgs/Int8.h>
#include <std_msgs/UInt8.h>
#include <std_msgs/Bool.h>

ros::NodeHandle nh;

unsigned long timeout_period = 2000;    // milliseconds of no updates before stopping
//unsigned long timeout_period = 11000;    // milliseconds of no updates before stopping
unsigned long last_activity;

char buffer[100];

int loop_count = 0;

int pt = 1;

int steering_left_pin = 4;
int steering_right_pin = 5;
int steering_position_pin = 1;
double steering_position = 128 << 2 ;

// double steering_desired = 128 << 2;
// disable steering
double steering_desired = 0;

double steering_correction = 0;
// PID steering_PID(&steering_position, &steering_correction, &steering_desired, 0.1, 0.02, 0.01, DIRECT);
//PID steering_PID(&steering_position, &steering_correction, &steering_desired, 0.6, 0.02, 0.01, DIRECT);
PID steering_PID(&steering_position, &steering_correction, &steering_desired, 0.6, 0.01, 0.01, DIRECT);

int shuttle_aft_pin = 2;
int shuttle_fore_pin = 3;
int shuttle_position_pin = 0;
double shuttle_position = 128 << 2;
double shuttle_desired = 128 << 2;
double shuttle_correction = 0;
PID shuttle_PID(&shuttle_position, &shuttle_correction, &shuttle_desired, 5, 1, 0, DIRECT);

int shuttle_speed = 0;
int steering_speed = 0;

int ignition_pin = 14;

int hitch_down_pin = 15;
int hitch_up_pin = 16;

int throttle_in_pin = 2;
int throttle_out_pin = 10;

int shuttle_lever_pin = 3;
 
std_msgs::UInt8 steering_position_message;
ros::Publisher steering_pub("/steering/get", &steering_position_message);
ros::Subscriber<std_msgs::UInt8> steering_sub("/steering/put", command_steering);

std_msgs::UInt8 shuttle_position_message;
ros::Publisher shuttle_pub("/transmission/shuttle/get", &shuttle_position_message);
ros::Subscriber<std_msgs::UInt8> shuttle_sub("/transmission/shuttle/put", command_shuttle);

std_msgs::Bool ignition_enable_message;
ros::Publisher ignition_pub("/ignition/get", &ignition_enable_message);
ros::Subscriber<std_msgs::Bool> ignition_sub("/ignition/put", command_ignition);

std_msgs::UInt8 hitch_message;
ros::Publisher hitch_pub("/hitch/get", &hitch_message);
ros::Subscriber<std_msgs::UInt8> hitch_sub("/hitch/put", command_hitch);

std_msgs::UInt8 throttle_message;
ros::Publisher throttle_pub("/throttle/get", &throttle_message);
ros::Subscriber<std_msgs::UInt8> throttle_sub("/throttle/put", command_throttle);

std_msgs::UInt8 shuttle_lever_message;
ros::Publisher shuttle_lever_pub("/transmission/shuttle/lever", &shuttle_lever_message);

std_msgs::Bool passthrough_message;
ros::Publisher passthrough_pub("/passthrough/get", &passthrough_message);
ros::Subscriber<std_msgs::Bool> passthrough_sub("/passthrough/put", command_passthrough);

void snooze() {
	//nh.logwarn("snooze()");
        
	//alarm_clock = millis() + timeout_period;
	
	last_activity = millis();

	//sprintf(buffer, "snooze %lu", last_activity); nh.logwarn(buffer);
}

void activity() {
	// nh.logwarn("activity()");

	digitalWrite(13, HIGH-digitalRead(13)); //toggle led
	snooze();
}


void steering(int i) {
	steering_desired = i << 2;

	if (steering_desired == 0) {
		steering_disable();
	}
}


void command_steering( const std_msgs::UInt8& cmd_msg) {
	// nh.logwarn("command_steering()");
	activity(); 

	if (pt) {
		return;
	}

	steering(cmd_msg.data);
}

void passthrough(byte i) {
	sprintf(buffer, "passthrough(%d)", i); nh.logwarn(buffer);

	pt = i;
	if (pt) {
		steering(0);
	}
}

void command_passthrough( const std_msgs::Bool& cmd_msg) {
	nh.logwarn("command_passthrough()");
	activity();

	passthrough(cmd_msg.data);
}

void ignition(byte i) {
	digitalWrite(ignition_pin, i);
	ignition_enable_message.data = i;

}

void command_ignition( const std_msgs::Bool& cmd_msg) {
	nh.logwarn("command_ignition()");

	ignition(cmd_msg.data);
}

void shuttle(int i) {
	shuttle_desired = (double)(i << 2);
}

void command_shuttle( const std_msgs::UInt8& cmd_msg) {
	//nh.logwarn("command_shuttle()");
	activity(); 

	if (pt) {
		return;
	}

	shuttle(cmd_msg.data);
}

void shuttle_correct() {
	shuttle_speed = abs(shuttle_correction);

	if (shuttle_speed < 100) {
		analogWrite(shuttle_aft_pin, 0);			
		analogWrite(shuttle_fore_pin, 0);
	} else if (shuttle_correction < 0) {
		// fore correction
		analogWrite(shuttle_aft_pin, 0);			
		analogWrite(shuttle_fore_pin, shuttle_speed);
	} else if (shuttle_correction > 0) {
		// aft correction
		analogWrite(shuttle_fore_pin, 0);
		analogWrite(shuttle_aft_pin, shuttle_speed);			
	}
}

void steering_disable() {
	analogWrite(steering_right_pin, 0);
	analogWrite(steering_left_pin, 0);
}

void steering_correct() {
	steering_speed = abs(steering_correction);
	
	if (steering_speed < 2) {
		analogWrite(steering_left_pin, 0);			
		analogWrite(steering_right_pin, 0);
	} else if (steering_correction > 0) {
		analogWrite(steering_right_pin, 0);
		analogWrite(steering_left_pin, 223 + steering_speed);			
	} else if (steering_correction < 0) {
		analogWrite(steering_left_pin, 0);			
		analogWrite(steering_right_pin, 225 + steering_speed);
	}
}

void hitch(int i) {
	if (i == 0) {
		digitalWrite(hitch_down_pin, 0);
		digitalWrite(hitch_up_pin, 0);
	} else if (i == 1) {
		digitalWrite(hitch_up_pin, 0);
		digitalWrite(hitch_down_pin, 1);
	} else if (i == 2) {
		digitalWrite(hitch_down_pin, 0);
		digitalWrite(hitch_up_pin, 1);
	}
	hitch_message.data = i;
}

void command_hitch( const std_msgs::UInt8& cmd_msg) {
	nh.logwarn("command_hitch()");
	activity();

	if (pt) {
		return;
	}

	hitch(cmd_msg.data);
}

void throttle(int i) {
	analogWrite(throttle_out_pin, i);
}

void command_throttle( const std_msgs::UInt8& cmd_msg) {
	// nh.logwarn("command_throttle()");
	activity();

	if (pt) {
		return;
	}

	throttle(cmd_msg.data);
}

void setup() {
	// Disable the ignition while we get started.
	pinMode(ignition_pin, OUTPUT);
	ignition(0);

	// Initialize hitch.
	pinMode(hitch_down_pin, OUTPUT);
	pinMode(hitch_up_pin, OUTPUT);
	hitch(0);		

	// Initialize shuttle.
	shuttle(128);

	// Initialize throttle.
	throttle(80);

	// Initialize rosserial link.
	nh.initNode();

	nh.advertise(ignition_pub);

	nh.advertise(steering_pub);
	nh.subscribe(steering_sub);

	nh.advertise(shuttle_pub);
	nh.subscribe(shuttle_sub);

	nh.advertise(hitch_pub);
	nh.subscribe(hitch_sub);

	nh.advertise(throttle_pub);
	nh.subscribe(throttle_sub);
	
	nh.advertise(shuttle_lever_pub);

	nh.advertise(passthrough_pub);
	nh.subscribe(passthrough_sub);

	//steering_PID.SetOutputLimits(-255.0, +255.0);
	//steering_PID.SetOutputLimits(-25.0, +25.0);
	//steering_PID.SetOutputLimits(-20.0, +20.0);
	//steering_PID.SetOutputLimits(-10.0, +10.0);
	steering_PID.SetOutputLimits(-30.0, +30.0);
	steering_PID.SetSampleTime(50);
	steering_PID.SetMode(AUTOMATIC);

	shuttle_PID.SetOutputLimits(-255.0, +255.0);
	shuttle_PID.SetSampleTime(50);
	shuttle_PID.SetMode(AUTOMATIC);

	// Enable the ignition.
	ignition(1);
	nh.subscribe(ignition_sub);
}

void loop() {
	// Do ROS stuff.
	nh.spinOnce();

	steering_position = analogRead(steering_position_pin);
	shuttle_position = analogRead(shuttle_position_pin);

	int shuttle_lever_reading = int(analogRead(shuttle_lever_pin)) >> 2;
	int throttle_reading = int(analogRead(throttle_in_pin)) >> 2;

	// We always control the shuttle, even in passthrough.
	shuttle_PID.Compute();
	shuttle_correct();


	if (!pt and ((millis() - last_activity) > timeout_period)) {
		sprintf(buffer, "%d, %lu, %lu, %lu", pt, millis(), last_activity, millis() - last_activity); nh.logwarn(buffer);
		passthrough(1);
	}

	// for testing
	//delay(500);

	if (pt)  {
		steering(0);

		throttle(throttle_reading);

		/* yet another hardcoded mess... */
		if (shuttle_lever_reading < 45) {
			shuttle(210);
		} else if (shuttle_lever_reading > 135) {
			shuttle(40);
		} else {
			shuttle(128);
		}
	} else {
		//sprintf(buffer, "steer"); nh.logwarn(buffer);

		if (steering_desired != 0) {
			steering_PID.Compute();
			steering_correct();
		}
	}

	loop_count += 1;
	if (loop_count % 1000 == 0) {
/*
		if (0) {
			sprintf(
				message, "%03d/%03d/%03d %03d/%03d/%03d", 
				int(steering_position) >> 2, int(steering_desired) >> 2, int(steering_correction),
				int(shuttle_position) >> 2, int(shuttle_desired) >> 2, int(shuttle_correction)
			);
		}

		dtostrf(shuttle_position, 6, 2, message);
		sprintf(message + strlen(message), "/");

		dtostrf(shuttle_desired, 6, 2, message + strlen(message));
		sprintf(message + strlen(message), "/");

		dtostrf(shuttle_correction, 6, 2, message + strlen(message));

		sprintf(message + strlen(message), "/%03d - ", shuttle_speed);




		dtostrf(steering_position, 6, 2, message + strlen(message));
		sprintf(message + strlen(message), "/");

		dtostrf(steering_desired, 6, 2, message + strlen(message));
		sprintf(message + strlen(message), "/");

		dtostrf(steering_correction, 6, 2, message + strlen(message));

		sprintf(message + strlen(message), "/%03d", steering_speed);



		nh.logwarn(message);

*/

		steering_position_message.data = int(steering_position) >> 2;
		steering_pub.publish( &steering_position_message );

		shuttle_position_message.data = int(shuttle_position) >> 2;
		shuttle_pub.publish( &shuttle_position_message );

		ignition_pub.publish( &ignition_enable_message );
		hitch_pub.publish( &hitch_message );

		throttle_message.data = throttle_reading;
		throttle_pub.publish( &throttle_message );

		shuttle_lever_message.data = shuttle_lever_reading;
		shuttle_lever_pub.publish( &shuttle_lever_message );

		passthrough_message.data = pt;
		passthrough_pub.publish( &passthrough_message );
	}


	delay(1);
}

