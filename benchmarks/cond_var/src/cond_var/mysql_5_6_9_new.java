package cond_var;

// Communication deadlock dumbed down version from Unhang paper, figure 3:
// https://dl.acm.org/doi/epdf/10.1145/3533767.3534377

public class mysql_5_6_9_new {
    static final Object l1 = new Object();
    static final Object l2 = new Object();
    
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
                }
            }
        }
    }
}