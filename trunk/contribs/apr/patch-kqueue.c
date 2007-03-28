--- /home/mmolteni/src/dfp/external-libs/apr-1.2.7/poll/unix/kqueue.c	Wed Mar 15 22:27:05 2006
+++ kqueue.c	Fri Aug 18 16:51:06 2006
@@ -240,13 +240,13 @@ APR_DECLARE(apr_status_t) apr_pollset_po
 
     if (timeout < 0) {
         tvptr = NULL;
     }
     else {
         tv.tv_sec = (long) apr_time_sec(timeout);
-        tv.tv_nsec = (long) apr_time_msec(timeout);
+        tv.tv_nsec = (long) apr_time_usec(timeout) * 1000;
         tvptr = &tv;
     }
 
     ret = kevent(pollset->kqueue_fd, NULL, 0, pollset->ke_set, pollset->nalloc,
                 tvptr);
     (*num) = ret;
