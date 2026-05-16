package cond_var;

// Communication deadlock dumbed down version 
// https://www.cs.princeton.edu/courses/archive/fall10/cos318/lectures/Deadlocks

// Actual bug:
// https://bz.apache.org/bugzilla/show_bug.cgi?id=42031


public class apache_42031{
    static final Object timeout = new Object();
    static final Object idlers = new Object();

    public static void main(String[] args){
        Thread t1 = new WorkerThread();
        t1.start();
    
        Thread t2 = new ListenerThread();
        t2.start();

        try{
            t1.join();
            t2.join();
        } catch(Exception e){
            System.out.println(e);
        }
    }

    static class ListenerThread extends Thread{
        public void run(){
            synchronized (timeout) {
                synchronized (idlers) {
                    try{
                        idlers.wait();
                    } catch(Exception e){
                        System.out.println(e);
                    }
                }
            }
        }
    }

    static class WorkerThread extends Thread{
        public void run() {
            synchronized (timeout) {
            }

            try{
                Thread.sleep(3);
            } catch (Exception e){
                System.out.println(e);
            }
            
            synchronized(idlers){
                idlers.notify();
            }
        }
    }
}