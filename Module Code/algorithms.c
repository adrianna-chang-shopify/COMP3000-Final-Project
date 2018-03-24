// Round robin algorithm

int thread_fn(void * v) {
  int counter = 0; //Ensure we don't get stuck in loop if passengers aren't being created properly
  // First pickUp variables
  int dist_to_origin = firstOrigin - elevatorCar.current_floor->id;
  int i;

  int elevatorDirection = 1; // 1 = up, -1 = down

  // Main loop to run elevator
  printk(KERN_INFO "In elevator-thread");
  while (firstOrigin < 0 && counter < 10){
    printk(KERN_INFO "Waiting for passengers!\n");
    msleep(1000);
    counter++;
  }

  printk(KERN_INFO "FirstOrigin: %d\n", firstOrigin);

  // First pickUp
  for(i=0; i<dist_to_origin; i++) {
    elevatorUp();
  }

  while(existsPassengerNode() > 0) {
    printk(KERN_INFO "Current floor: %d\n", elevatorCar.current_floor->id);

    // Algorithm
    // Check for pick up
    if (shaftArray[floor_to_check].startQueue != NULL) {
      pickUp();
    }

    // Check for direction changes
    if (elevatorCar.current_floor->id == 0) {
      printk(KERN_INFO "Changing direction to UP\n");
      elevatorDirection = 1;
    }
    else if (elevatorCar.current_floor->id == 6) {
      printk(KERN_INFO "Changing direction to DOWN\n");
      elevatorDirection = -1;
    }

    // Move elevator
    if (elevatorDirection == 1) {
      elevatorUp();
    }
    else if (elevatorDirection == -1) {
      elevatorDown();
    }

    // Check for drop off
    if (elevatorCar.passengerArray[floor_to_check] != NULL) {
      dropOff();
    }

    if (existsPassengerNode() == 0) {
      printk(KERN_INFO "Sleeping while waiting for passengers\n");
      msleep(1000);
    }
  }

  //print results!

  return 0;
}

// FCFS algorithm

int thread_fn(void * v) {
  int counter = 0; //Ensure we don't get stuck in loop if passengers aren't being created properly
  // First pickUp variables
  int dist_to_origin = firstOrigin - elevatorCar.current_floor->id;
  int i;
  int next_destination, floor_delta;
  int highest_priority, current_passenger_priority;

  // Main loop to run elevator
  printk(KERN_INFO "In elevator-thread");
  while (firstOrigin < 0 && counter < 10){
    printk(KERN_INFO "Waiting for passengers!\n");
    msleep(1000);
    counter++;
  }

  printk(KERN_INFO "FirstOrigin: %d\n", firstOrigin);

  // First pickUp
  for(i=0; i<dist_to_origin; i++) {
    elevatorUp();
  }

  // Main loop
  while(existsPassengerNode() > 0) {
    // pick up passengers on current floor
    pickUp();

    // Check priority of passengers in elevator and determine next destination for elevator to move to
    i = 0;
    while(elevatorCar.passengerArray[i] == NULL) {
      i++;
    }

    highest_priority = elevatorCar.passengerArray[i]->id;
    next_destination = i;

    for(i+1; i<NUM_FLOORS; i++) {
      current_passenger_priority = elevatorCar.passengerArray[i]->id;
      if (current_passenger_priority < highest_priority) {
        highest_priority = current_passenger_priority;
        next_destination = i;
      }
    }

    // Move to next destination
    floor_delta = elevatorCar.current_floor->id - next_destination
    if ( floor_delta > 0) {
      for (i=0; i<floor_delta; i++) {
        elevatorDown();
      }
    }
    else {
      floor_delta *= -1;
      for (i=0; i<floor_delta; i++) {
        elevatorUp();
      }
    }

    // Drop off
    dropOff();

    if (existsPassengerNode() == 0) {
      printk(KERN_INFO "Sleeping while waiting for passengers\n");
      msleep(1000);
    }
  }

  //print results!

  return 0;
}

// Shortest distance first algorithm

int thread_fn(void * v) {
  int counter = 0; //Ensure we don't get stuck in loop if passengers aren't being created properly
  // First pickUp variables
  int dist_to_origin = firstOrigin - elevatorCar.current_floor->id;
  int i;
  int floor_up_priority, floor_down_priority;

  // Main loop to run elevator
  printk(KERN_INFO "In elevator-thread");
  while (firstOrigin < 0 && counter < 10){
    printk(KERN_INFO "Waiting for passengers!\n");
    msleep(1000);
    counter++;
  }

  printk(KERN_INFO "FirstOrigin: %d\n", firstOrigin);

  // First pickUp
  for(i=0; i<dist_to_origin; i++) {
    elevatorUp();
  }

  // Main loop
  while(existsPassengerNode() > 0) {
    // pick up passengers on current floor
    pickUp();

    //Finding closest floor for drop off
    for(i=1; i<NUM_FLOORS-1; i++) {
      floor_up_priority = checkPriority((elevatorCar.current_floor->id) + i);
      floor_down_priority = checkPriority((elevatorCar.current_floor->id) - i);

      if (floor_up_priority != 0 && floor_down_priority == 0) {
        elevatorUp();
        break;
      }
      else if (floor_up_priority == 0 && floor_down_priority != 0) {
        elevatorDown();
        break;
      }
      else if (floor_up_priority != 0 && floor_down_priority != 0) {
        if (floor_up_priority < floor_down_priority) {
          elevatorUp();
        }
        else {
          elevatorDown();
        }
        break;
      }
    }

    dropOff();

    if (existsPassengerNode() == 0) {
      printk(KERN_INFO "Sleeping while waiting for passengers\n");
      msleep(1000);
    }
  }

  //print results!

  return 0;
}

int checkPriority(int floor_num) {
  if (floor_num < 0 || floor_num > 5) {
    return 0;
  }

  if (elevatorCar.passengerArray[floor_num] == NULL) {
    return 0;
  }
  else {
    elevatorCar.passengerArray[floor_num]->id;
  }
}
