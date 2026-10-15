import robocode.AdvancedRobot;\n\npublic class BlockRobot extends AdvancedRobot {
    @Override
    public void run() {
        setAhead(100);
        execute();
        setTurnRight(90);
        execute();
        setFire(2);
        execute();
    }
}
