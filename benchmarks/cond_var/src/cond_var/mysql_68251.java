package cond_var;
// Actual bug:
// https://bugs.mysql.com/bug.php?id=68251

public class mysql_68251 {

    private static final Object lock_commit = new Object();
    private static final Object lock_log = new Object();
    private static final Object position_update = new Object();

    public static void main(String[] args){
        new T1().start();
        new T2().start();
        new T3().start();
    }

    // Thread 1: Client doing commit
    static class T1 extends Thread{
        public void run(){
            pause(300);
            synchronized (lock_commit) {
                synchronized (position_update) {
                    try {
                        position_update.wait(); 
                    } catch (InterruptedException ignored) {}
                }
            }
        }
    }

        
    // Thread 2: Client rotating binlog
    static class T2 extends Thread{
        public void run(){
            synchronized (lock_log) {
                synchronized (lock_commit) {}
            }
        }
    }

    // Thread 3: binlog_dump thread
    static class T3 extends Thread{
        public void run(){
            pause(300);
            synchronized (lock_log) {
                synchronized (position_update) {
                    position_update.notifyAll();
                }
            }
        }
    }

    private static void pause(long millis) {
        try { Thread.sleep(millis); } catch (InterruptedException ignored) {}
    }
}