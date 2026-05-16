package cond_var;

// Communication deadlock dumbed down version from Unhang paper:
// https://dl.acm.org/doi/epdf/10.1145/3533767.3534377

// Actual bug:
// https://bugs.mysql.com/bug.php?id=68251
public class mysql_68251 {
    // Standard objects act as our locks/monitors
    static final Object l1 = new Object();
    static final Object l2 = new Object();
    
    // This single object replaces both l_cv1 and cv1.
    static final Object cv1 = new Object();

    public static void main(String[] args){
        new T1().start();
        new T2().start();
        new T3().start();
    }

    // Thread t1
    static class T1 extends Thread{
        public void run(){
            synchronized (l1) {
                synchronized (cv1) {
                    try {
                        cv1.wait(); 
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }
    }

    // Thread t2
    static class T2 extends Thread{
        public void run() {
            synchronized (l2) {
                // Acquires and immediately releases l2 when the block ends
            }

            synchronized (cv1) {
                cv1.notify();
            }
        }
    }

    // Thread t3
    static class T3 extends Thread{
        public void run() {
            synchronized (l2) {
                synchronized (l1) {
                    // Critical section operations would go here
                }
            }
        }
    }
}