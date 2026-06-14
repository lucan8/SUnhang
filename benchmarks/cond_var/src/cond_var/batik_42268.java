package cond_var;
// Bug found here: https://bz.apache.org/bugzilla/show_bug.cgi?id=42268
// The functions that were checked can be found here: https://github.com/openjdk/jdk/tree/jdk7-b24
// THIS IS A VERY DUMBED DOWN VERSION OF IT

// To clearly identify the deadlock, you should use both the stack trace from the bug report
// and the source code of 
// InvocationEvents: dispatch(), EventDispatchThread:pumpOneEventForFilters()
// RunnableEventQueue: InvokeAndWait and postEvent (especially postEventPrivate and postEvent with 2 params)
// This is a consumer-producer situation with EventDispatchThread as the consumer and RunnableEventQueue as a producer
// Extra comments were added to this dumbed down version to make it more clear where each obj/function comes from
// The sleep and other of operaitons are like this in order to not reproduce the deadlock

public class batik_42268 {

    // Represents the external resource lock (Batik's RunnableQueue)
    private static final Object runnable_queue_lock = new Object();

    // Represents the local lock created inside EventQueue.invokeAndWait()
    // that is used to create InvocationEvent
    private static final Object awt_invocation_lock = new Object();

    public static void main(String[] args) {
        new T1().start();
        new T2().start();
    }

    // Thread 1: Batik RunnableQueue Thread
    static class T1 extends Thread{
        public void run(){
            pause(300);
            synchronized (runnable_queue_lock) {
                // Entering EventQueue.invokeAndWait()
                synchronized (awt_invocation_lock) {
                    // postEvent() happens here
                    try {
                        awt_invocation_lock.wait();
                    } catch (InterruptedException ignored) {}
                }
            }
        }
    }

    // Thread 2: Swing Event Dispatch Thread
    static class T2 extends Thread{
        public void run(){
            // Executing InvocationEvent.dispatch()
            try {
                // Executing runnable.run()
                synchronized (runnable_queue_lock) {
                }
                pause(1000);
            } finally {
                synchronized (awt_invocation_lock) {
                    awt_invocation_lock.notifyAll();
                }
            }
        }
    }

    private static void pause(long millis) {
        try { Thread.sleep(millis); } catch (InterruptedException ignored) {}
    }
}