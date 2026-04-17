package cond_var;

// Communication deadlock dumbed down version from the Unhang paper 
// https://dl.acm.org/doi/epdf/10.1145/3533767.3534377


public class memcached_567{
    static final Object lock = new Object();
    static final Object cv = new Object();
    static volatile int count = 0;
    static volatile int N = 2;
    static volatile int rounds = 3;

    public static void main(String[] args){
        Thread t1 = new WorkerThread();
        t1.start();
    
        Thread t2 = new MaintenanceThread();
        t2.start();

        try{
            t1.join();
            t2.join();
        } catch(Exception e){
            System.out.println(e);
        }
    }

    static class WorkerThread extends Thread{
        public void run(){
            while (rounds > 0){
                synchronized (cv) {
                    count += 1;
                    cv.notify();
                }

                synchronized(lock){

                }

                try{
                    Thread.sleep(1);
                } catch (Exception e){
                    System.out.println(e);
                }
                rounds -= 1;
            }
        }
    }

    static class MaintenanceThread extends Thread{
        public void run() {
            synchronized (lock) {
                synchronized (cv){
                    while (count < N){
                        try{
                            cv.wait();
                        } catch (Exception e){
                            System.out.println(e);
                        }
                    }
                }
            }
        }
    }
}