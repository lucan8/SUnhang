package cond_var;

public class cond_var_fp_pap_idiomatic {

    private static final Object l1 = new Object();
    private static final Object l2 = new Object();
    private static final Object l_cv = new Object();
    
    private static boolean cond = false; 

    public static void main(String[] args) {
        new T1().start();
        new T2().start();
    }

    static class T1 extends Thread{
        public void run() {
            synchronized (l_cv) {
                while (!cond) { 
                    try {
                        l_cv.wait(); 
                    } catch (InterruptedException ignored) {}
                }
            }

            synchronized (l1) {
                synchronized (l2) {}
            }
        }
    }

    static class T2 extends Thread{
        public void run() {
            pause(200);
            synchronized (l2) {
                synchronized (l1) {}
            }

            synchronized (l_cv) {
                cond = true;
                l_cv.notifyAll();
            }
        }
    }

    private static void pause(long millis) {
        try { Thread.sleep(millis); } catch (InterruptedException ignored) {}
    }
}