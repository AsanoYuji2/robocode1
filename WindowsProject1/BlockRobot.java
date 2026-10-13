import robocode.AdvancedRobot;

public class BlockRobot extends AdvancedRobot {
    @Override
    public void run() {
        setFire(2);
        execute();
        setTurnRight(90);
        execute();
        setAhead(100);
        execute();
    }
}
