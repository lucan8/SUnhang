import rr.annotations.Abbrev;
import rr.event.AccessEvent;
import rr.event.AcquireEvent;
import rr.event.AllocateEvent;
import rr.event.JoinEvent;
import rr.event.MethodEvent;
import rr.event.NotifyEvent;
import rr.event.ReleaseEvent;
import rr.event.StartEvent;
import rr.event.WaitEvent;
import rr.tool.Tool;
import acme.util.option.CommandLine;

@Abbrev("STD")
public class StdTraceTool extends Tool {

    // Synchronization lock to ensure console output doesn't interleave
    private static final Object printLock = new Object();

    public StdTraceTool(String name, Tool next, CommandLine commandLine) {
        super(name, next, commandLine);
    }

    @Override
    public void acquire(AcquireEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            System.out.println(tid + "|acq(" + lockId + ")|" + iid);
        }
        super.acquire(e);
    }

    @Override
    public void release(ReleaseEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            System.out.println(tid + "|rel(" + lockId + ")|" + iid);
        }
        super.release(e);
    }

    @Override
    public void access(AccessEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int objId = System.identityHashCode(e.getTarget());
            int iid = e.getAccessInfo() != null ? e.getAccessInfo().getId() : 0;
            
            if (e.isWrite()) {
                System.out.println(tid + "|write(" + objId + ")|" + iid);
            } else {
                System.out.println(tid + "|read(" + objId + ")|" + iid);
            }
        }
        super.access(e);
    }


    @Override
    public void preStart(StartEvent e) {
        synchronized (printLock) {
            int parentTid = e.getThread().getTid();
            int childTid = e.getNewThread().getTid();
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            System.out.println(parentTid + "|fork(" + childTid + ")|" + iid);
        }
        super.preStart(e);
    }

    @Override
    public void postJoin(JoinEvent e) {
        synchronized (printLock) {
            int parentTid = e.getThread().getTid();
            int childTid = e.getJoiningThread().getTid();
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            System.out.println(parentTid + "|join(" + childTid + ")|" + iid);
        }
        super.postJoin(e);
    }

    @Override
    public void preWait(WaitEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            // Release the lock, then wait
            System.out.println(tid + "|rel(" + lockId + ")|" + iid);
            System.out.println(tid + "|wait(" + lockId + ")|" + iid);
        }
        super.preWait(e);
    }

    @Override
    public void postWait(WaitEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            // Re-acquire the lock upon waking up
            System.out.println(tid + "|acq(" + lockId + ")|" + iid);
        }
        super.postWait(e);
    }

    @Override
    public void preNotify(NotifyEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            if (e.isNotifyAll()) {
                System.out.println(tid + "|notifyAll(" + lockId + ")|" + iid);
            } else {
                System.out.println(tid + "|notify(" + lockId + ")|" + iid);
            }
        }
        super.preNotify(e);
    }
}