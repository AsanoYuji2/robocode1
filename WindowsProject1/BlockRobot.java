import robocode.AdvancedRobot;

public class BlockRobot extends AdvancedRobot
{
    @Override
    public void run()
    {
        while(true)
        {
            setAhead(100);
            execute();
            setTurnRight(90);
            execute();
            fire(2);
            execute();
        }
    }
}
