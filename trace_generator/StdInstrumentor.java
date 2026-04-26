import rr.annotations.Abbrev;
import rr.event.AccessEvent;
import rr.event.AcquireEvent;
import rr.event.JoinEvent;
import rr.event.NotifyEvent;
import rr.event.ReleaseEvent;
import rr.event.StartEvent;
import rr.event.WaitEvent;
import rr.tool.Tool;
import acme.util.option.CommandLine;

import java.io.PrintWriter;
import java.io.FileWriter;
import java.io.IOException;

@Abbrev("STD")
public class StdInstrumentor extends Tool {
    private static final Object printLock = new Object();
    private static PrintWriter out;
    static {
        try {
            // Ask the JVM for the custom property injected via bash
            // Provide a fallback name ("default_trace.std") just in case
            String traceFilePath = System.getProperty("std.trace.file", "default_trace.std");
            out = new PrintWriter(new FileWriter(traceFilePath, false), true);
            
            System.out.println("[STD Instrumentor] Routing trace output to: " + traceFilePath);
        } catch (IOException e) {
            throw new RuntimeException("Failed to open trace file", e);
        }
    }

    public StdInstrumentor(String name, Tool next, CommandLine commandLine) {
        super(name, next, commandLine);
    }

    @Override
    public void acquire(AcquireEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            out.println("T" + tid + "|acq(" + lockId + ")|" + iid);
        }
        super.acquire(e);
    }

    @Override
    public void release(ReleaseEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            out.println("T" + tid + "|rel(" + lockId + ")|" + iid);
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
                out.println("T" + tid + "|w(" + objId + ")|" + iid);
            } else {
                out.println("T" + tid + "|r(" + objId + ")|" + iid);
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
            
            out.println("T" + parentTid + "|fork(" + childTid + ")|" + iid);
        }
        super.preStart(e);
    }

    @Override
    public void postJoin(JoinEvent e) {
        synchronized (printLock) {
            int parentTid = e.getThread().getTid();
            int childTid = e.getJoiningThread().getTid();
            int iid = e.getInfo() != null ? e.getInfo().getId() : 0;
            
            out.println("T" + parentTid + "|join(" + childTid + ")|" + iid);
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
            out.println("T" + tid + "|rel(" + lockId + ")|" + iid);
            out.println("T" + tid + "|wait(" + lockId + ")|" + iid);
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
            out.println("T" + tid + "|acq(" + lockId + ")|" + iid);
        }
        super.postWait(e);
    }

    @Override
    public void preNotify(NotifyEvent e) {
        synchronized (printLock) {
            int tid = e.getThread().getTid();
            int lockId = System.identityHashCode(e.getLock().getLock());
            int iid = 0;
            
            if (e.isNotifyAll()) {
                out.println("T" + tid + "|notifyAll(" + lockId + ")|" + iid);
            } else {
                out.println("T" + tid + "|notify(" + lockId + ")|" + iid);
            }
        }
        super.preNotify(e);
    }

    @Override
    public void fini() {
        // Guaranteed to run when RoadRunner shuts down to safely close the file
        synchronized (printLock) {
            if (out != null) {
                out.close();
            }
        }
        super.fini();
    }
}