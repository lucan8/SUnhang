package cond_var;

// Communication deadlock dumbed down version(by Alfranio Correia) 
// https://bugs.mysql.com/bug.php?id=50038

// Deadlock situation operations order:
// T1: 1. flush trx changes
// T2: 2. flush log
// T1: 3. flush non trx changes

public class mysql_50038{
    static final Object log = new Object();
    static final Object prep_xids = new Object();
    static volatile int prep_xids_cnt = 0;

    public static void main(String[] args){
        Thread t1 = new FlushChangesThread();
        t1.start();
    
        Thread t2 = new FlushLogThread();
        t2.start();

        try{
            t1.join();
            t2.join();
        } catch(Exception e){
            System.out.println(e);
        }
    }

    static class FlushChangesThread extends Thread{
        private void flush_trx_changes(){
            synchronized(log){
                synchronized(prep_xids){
                    prep_xids_cnt += 1;
                }
            }
        }

        private void flush_non_trx_changes(){
            synchronized(log){

            }
        }

        private void unlog(){
            synchronized(prep_xids){
                prep_xids_cnt -= 1;
                prep_xids.notify();
            }
        }

        public void run(){
            flush_trx_changes();
            flush_non_trx_changes();
            try{
                Thread.sleep(3);
            } catch (Exception e){
                System.out.println(e);
            }
            unlog();
        }
    }

    static class FlushLogThread extends Thread{
        public void run(){
            synchronized(log){
                synchronized(prep_xids){
                    try {
                        while (prep_xids_cnt > 0){
                            prep_xids.wait();
                        }
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }
    }
}