

from controller import Robot


BASE_SPEED = 2.6       
KP = 2.8                
KD = 3.0               
ERROR_SMOOTH = 0.35     
CORNER_SLOWDOWN = 0.60 
MAX_SPEED = 8.0         

DEADBAND = 0.30         
LOST_TOTAL = 0.12      
SEARCH_SPIN = 3.0       
SEARCH_PIVOT_STEPS = 250  
                         
                          
                          
SEARCH_CREEP = 0.6      

SETTLE_STEPS = 20       
SWEEP_SPIN = 1.5        
SWEEP_QUARTER = 45      
MIN_CONTRAST = 25.0     

DEBUG_EVERY = 60        

SENSORS = ("ir_left", "ir_center", "ir_right")
LEFT_WHEELS = ("wheel_fl", "wheel_rl")
RIGHT_WHEELS = ("wheel_fr", "wheel_rr")


def clamp(value, low, high):
    return max(low, min(high, value))


def percentile(sorted_values, fraction):
    if not sorted_values:
        return 0.0
    i = int(fraction * (len(sorted_values) - 1))
    return sorted_values[clamp(i, 0, len(sorted_values) - 1)]


def median(values):
    if not values:
        return 0.0
    ordered = sorted(values)
    mid = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[mid]
    return (ordered[mid - 1] + ordered[mid]) / 2.0


class LineFollower:
    def __init__(self):
        self.robot = Robot()
        self.timestep = int(self.robot.getBasicTimeStep())

        self.left_motors = [self.robot.getDevice(n) for n in LEFT_WHEELS]
        self.right_motors = [self.robot.getDevice(n) for n in RIGHT_WHEELS]
        for motor in self.left_motors + self.right_motors:
            motor.setPosition(float("inf"))   
            motor.setVelocity(0.0)

        self.sensors = []
        for name in SENSORS:
            sensor = self.robot.getDevice(name)
            sensor.enable(self.timestep)
            self.sensors.append(sensor)

        self.line_ref = None     
        self.floor_ref = None    
        self.blind = False       
        self.lost_steps = 0
        self.smooth_error = 0.0
        self.prev_error = 0.0
        self.last_error = 0.0
        self.step_count = 0

    
    def read_raw(self):
        return [s.getValue() for s in self.sensors]

    def drive(self, left_speed, right_speed):
        left_speed = clamp(left_speed, -MAX_SPEED, MAX_SPEED)
        right_speed = clamp(right_speed, -MAX_SPEED, MAX_SPEED)
        for motor in self.left_motors:
            motor.setVelocity(left_speed)
        for motor in self.right_motors:
            motor.setVelocity(right_speed)

    def _spin(self, steps, left_speed, right_speed, samples):
        done = 0
        while done < steps and self.robot.step(self.timestep) != -1:
            self.drive(left_speed, right_speed)
            samples.extend(self.read_raw())
            done += 1

    
    def calibrate(self):
        """Sweep left and right to learn what line and floor read like."""
        
        centre_samples = []
        taken = 0
        while taken < SETTLE_STEPS and self.robot.step(self.timestep) != -1:
            self.drive(0.0, 0.0)
            centre_samples.append(self.read_raw()[1])
            taken += 1
        on_line = median(centre_samples)

       
        samples = []
        self._spin(SWEEP_QUARTER, -SWEEP_SPIN, SWEEP_SPIN, samples)
        self._spin(SWEEP_QUARTER * 2, SWEEP_SPIN, -SWEEP_SPIN, samples)
        self._spin(SWEEP_QUARTER, -SWEEP_SPIN, SWEEP_SPIN, samples)
        self.drive(0.0, 0.0)

        ordered = sorted(samples)
        low_end = percentile(ordered, 0.02)    
        high_end = percentile(ordered, 0.98)   
        midpoint = (low_end + high_end) / 2.0

        line_is_high = abs(on_line - high_end) < abs(on_line - low_end)
        if line_is_high:
            self.line_ref = high_end
            
            self.floor_ref = median([v for v in ordered if v < midpoint])
        else:
            self.line_ref = low_end
            self.floor_ref = median([v for v in ordered if v > midpoint])
        contrast = self.line_ref - self.floor_ref

        print("")
        print("=" * 60)
        print("  CALIBRATION SWEEP   (%d samples)" % len(samples))
        print("  on the line at rest : %8.1f" % on_line)
        print("  robust range        : %8.1f  ..  %.1f" % (low_end, high_end))
        print("  -> line reads       : %8.1f" % self.line_ref)
        print("  -> typical floor    : %8.1f   (median, not the extreme)"
              % self.floor_ref)
        print("  -> contrast         : %+8.1f" % contrast)
        print("=" * 60)

        if abs(contrast) < MIN_CONTRAST:
            self.blind = True
            print("  SENSORS CANNOT SEE THE LINE - not driving.")
            print("")
            if max(abs(low_end), abs(high_end)) < 20.0:
                print("  Every reading is 0, so both surfaces fall off the END")
                print("  of the lookupTable. A Webots infra-red sensor divides")
                print("  the distance by the surface's reflection factor first,")
                print("  so a dark line reports several times its real distance.")
                print("")
                print("  FIX: in worlds/task1b.wbt each DistanceSensor needs")
                print("       lookupTable [ 0 1000 0, 0.120 0 0 ]")
                print("       If yours says 0.030, you are running the old")
                print("       world file - replace it and reload (Ctrl+Shift+T).")
            else:
                print("  Readings are non-zero but barely change over the line.")
                print("  Check the floor and line baseColor differ strongly and")
                print("  that nothing is modelled under the nose blocking the")
                print("  beams (infra-red rays hit the robot's own geometry).")
            print("=" * 60)
        else:
            print("  line reads %s than the floor. Following it now."
                  % ("HIGHER" if contrast > 0 else "LOWER"))
            print("=" * 60)

    def brightness(self, raw):
        
        span = self.line_ref - self.floor_ref
        if abs(span) < 1e-6:
            return 0.0
        value = (raw - self.floor_ref) / span
        value = (value - DEADBAND) / (1.0 - DEADBAND)
        return clamp(value, 0.0, 1.0)

   
    def step(self):
        left_raw, centre_raw, right_raw = self.read_raw()

        if self.blind:
            self.drive(0.0, 0.0)
            self.step_count += 1
            if self.step_count % 60 == 0:
                print("parked - raw readings L=%.1f C=%.1f R=%.1f"
                      % (left_raw, centre_raw, right_raw))
            return

        w_left = self.brightness(left_raw)
        w_centre = self.brightness(centre_raw)
        w_right = self.brightness(right_raw)
        total = w_left + w_centre + w_right

        if total < LOST_TOTAL:
           
            self.lost_steps += 1
            direction = 1.0 if self.last_error >= 0 else -1.0
            spin = SEARCH_SPIN * direction
            creep = 0.0 if self.lost_steps < SEARCH_PIVOT_STEPS else SEARCH_CREEP
            self.drive(creep + spin, creep - spin)
            state = "LOST"
            error = self.last_error
        else:
            self.lost_steps = 0
           
            error = (w_right - w_left) / total
            self.last_error = error

           
           
            self.smooth_error += ERROR_SMOOTH * (error - self.smooth_error)
            turn = KP * self.smooth_error + KD * (self.smooth_error
                                                  - self.prev_error)
            base = BASE_SPEED * (1.0 - CORNER_SLOWDOWN
                                 * min(abs(self.smooth_error), 1.0))
            self.drive(base + turn, base - turn)
            self.prev_error = self.smooth_error
            state = "TRACK"

        self.step_count += 1

        if DEBUG_EVERY and self.step_count % DEBUG_EVERY == 0:
            print("%-5s  raw %5.0f %5.0f %5.0f   line %.2f %.2f %.2f   err=%+.2f"
                  % (state, left_raw, centre_raw, right_raw,
                     w_left, w_centre, w_right, error))

    def run(self):
        print("Task 1b line follower - calibrating, please wait...")
        self.calibrate()
        while self.robot.step(self.timestep) != -1:
            self.step()


if __name__ == "__main__":
    LineFollower().run()
