package com.fazecast.jSerialComm;

import java.util.concurrent.ThreadFactory;

public class SerialPortThreadFactory {
    private static ThreadFactory INSTANCE = new ThreadFactory() {
        @Override
        public Thread newThread(Runnable r) {
            return new Thread(r);
        }
    };

    public static ThreadFactory get() {
        return INSTANCE;
    }

    /**
     * Use this method to supply custom thread factory
     * @param threadFactory
     */
    public static void set(ThreadFactory threadFactory) {
        SerialPortThreadFactory.INSTANCE = threadFactory;
    }
}
