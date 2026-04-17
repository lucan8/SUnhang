package cond_var;
public class cond_var_fn_idiomatic {
    // Standard objects act as our locks/monitors
    static final Object l1 = new Object();
    static final Object l2 = new Object();
    
    // This single object replaces both l_cv1 and cv1.
    static final Object cv1 = new Object();
    static volatile int cond = 0;
    
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
                        while (cond == 0){
                            cv1.wait();
                        }
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
                cond = 1;
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