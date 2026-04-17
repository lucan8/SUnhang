package cond_var;

public class cond_var_tn {
    // Standard objects act as our locks/monitors
    static final Object l1 = new Object();
    static final Object l2 = new Object();
    static volatile int signal;
    static volatile int var;

    public static void main(String[] args){
        var = 0;
        signal = 0;

        Thread t1 = new T1();
        t1.start();
        
        while (signal == 0);

        var = 1;
        Thread t2 = new T2();
        t2.start();

        try{
            t1.join();
            t2.join();
        } catch(Exception e){
            System.out.println(e);
        }
    }

    // Thread t1
    static class T1 extends Thread{
        public void run(){
            synchronized (l1) {
                if (var == 0){
                    synchronized (l2) {
                        signal = 1;
                        //Do other time consuming tasks....
                    }
                }
            }
        }
    }

    // Thread t2
    static class T2 extends Thread{
        public void run() {
             synchronized (l2) {
                if (var != 0){
                    synchronized (l1) {
                    }
                }
            }
        }
    }
}