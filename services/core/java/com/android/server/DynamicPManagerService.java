/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.os.Binder;
import android.util.Log;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileWriter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Iterator;
import android.os.SystemProperties;
import java.util.Timer;
import java.util.TimerTask;
import android.media.MediaPlayer;
import android.media.AudioManager;
import android.app.ActivityManagerNative;
import android.os.IDynamicPManager;
import android.os.DynamicPManager;

public class DynamicPManagerService extends IDynamicPManager.Stub {
    private static final String TAG = "DynamicPManagerService";

    private final Context mContext;
    /**
     * cpufreq governor
     */
    private final String CPU0FREQ_GOVERNOR_PATH = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";
    private final String CPU4FREQ_GOVERNOR_PATH = "/sys/devices/system/cpu/cpu4/cpufreq/scaling_governor";
    private final String LOWMEM_ADJ_PATH        = "/sys/module/lowmemorykiller/parameters/adj";
    private final String CPU0BOOT_LOCK_FILE = "/sys/devices/system/cpu/cpu0/cpufreq/boot_lock";
    private final String CPU4BOOT_LOCK_FILE = "/sys/devices/system/cpu/cpu4/cpufreq/boot_lock";
    private final String LOWMEM_MINFREE_PATH    ="/sys/module/lowmemorykiller/parameters/minfree";
    private final String CPU_GOVERNOR_FANTASYS = "fantasys";
    private final String CPU_GOVERNOR_INTERACTIVE = "interactive";
    private final String CPU_GOVERNOR_PERFORMANCE = "performance";
    private final String CPU_AUTO_HOTPLUG_FILE    = "/sys/kernel/autohotplug/enable";
    private final String BOOST_UPERF_4KLOCALVIDEO = DynamicPManager.BOOST_UPERF_4KLOCALVIDEO;
    private final String BOOST_UPERF_LOCALVIDEO   = DynamicPManager.BOOST_UPERF_LOCALVIDEO;
    private final String BOOST_UPERF_NORMAL      = DynamicPManager.BOOST_UPERF_NORMAL;
    private final String BOOST_UPERF_EXTREME     = DynamicPManager.BOOST_UPERF_EXTREME;
    private final String BOOST_UPERF_HOMENTER    = DynamicPManager.BOOST_UPERF_HOMENTER;
    private final String BOOST_UPERF_HOMEXIT     = DynamicPManager.BOOST_UPERF_HOMEXIT;
    private final String BOOST_UPERF_BGMUSIC     = DynamicPManager.BOOST_UPERF_BGMUSIC;
    private final String BOOST_UPERF_ROTATENTER  = DynamicPManager.BOOST_UPERF_ROTATENTER;
    private final String BOOST_UPERF_ROTATEXIT   = DynamicPManager.BOOST_UPERF_ROTATEXIT;
    private final String BOOST_UPERF_USBENTER    = DynamicPManager.BOOST_UPERF_USBENTER;
    private final String BOOST_UPERF_USBEXIT     = DynamicPManager.BOOST_UPERF_USBEXIT;
    private final String APP_DETECT_PROCESS_LIMIT_FILE_NAME = "mem_lim_list.conf";
    private final String syncVar       = "Syncvar";
    private final String LAUNCHER = "com.android.launcher";
    private String mAdj = null;
    private String mMinfree = null;
    private String mCurMode = BOOST_UPERF_EXTREME;
    private boolean DEBUG = false;
    private boolean Is_localvideo   = false;
    private boolean mSenctrl_enable = true;
    private boolean mSystemIsReady  = false;
    private boolean mIsLowMemDev    = false;

    /*
     * this condition means to full-screen
     */
    private int DEFAULT_MAX_LAYERS = 2;
    private int VIDEO_PLAYING  = 1;
    private int mCurSence_index = 1;
    private int DECODE_HW              = 1;
    private int mCurLayers             = 0;
    private int mOriginalLimit = -1;
    private int LauncherPid    = 0;
    private final int PROCESS_LIMITATION_NUM = 5;

    private Timer mTimer = null;
    private BoostUPerfTask mBoostUPerfTask = null;
    private BoostUPerfWatchDog mBoostUPerfWatchDog = null;
    private AudioManager mAudioService;
    private ActivityManager mActivityManager;
    private ArrayList<String> mProcessLimitAppList;

    MediaPlayer.MediaPlayerInfo minfo = new MediaPlayer.MediaPlayerInfo();
    MediaPlayer.MediaPlayerInfo ginfo = new MediaPlayer.MediaPlayerInfo();

    public DynamicPManagerService(Context context) {
        mContext = context;
        Log.i(TAG, "register boot completed receiver");

        IntentFilter boostFilter = new IntentFilter();
        boostFilter.addAction(Intent.ACTION_BOOST_UP_PERF);
        boostFilter.addAction(Intent.ACTION_SCREEN_ON);
        boostFilter.addAction(Intent.ACTION_SCREEN_OFF);
        boostFilter.addAction(Intent.ACTION_BOOT_COMPLETED);
        mContext.registerReceiver(new BoostUPerfBr(), boostFilter);
    }

    private void boostuperf_set_mode(String mode) {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_BOOST_UP_PERF);
        intent.putExtra("mode", mode);
        synchronized(syncVar) {
            set_boost_up_perf_mode(intent);
        }
    }

    class BoostUPerfWatchDog extends TimerTask {
        public void run() {
            int nlayers = SystemProperties.getInt("sys.boost_up_perf.layers", DEFAULT_MAX_LAYERS + 1);
            int num = MediaPlayer.getMediaPlayerList();
            int nPlayingHw = 0;

            if (DEBUG)
                Log.e(TAG, "BoostUPerfWatchDog-----------------");
            if (num > 0) {
                for (int i = 0; i < num; i++) {
                    int ret = MediaPlayer.getMediaPlayerInfo(i,minfo);
                    if (ret == 0 && minfo.codecType == DECODE_HW && minfo.playState == VIDEO_PLAYING) {
                        nPlayingHw++;
                        ginfo = minfo;
                    }
                }
            }
            /*
             * if it is not full-screen
             * boost up first and cancel watchdog, schedule boostUPerfTask
             */
            if (DEBUG)
                Log.e(TAG, "boostuperfWatchDog: nlayers:" + nlayers + ",nplayers:" + nPlayingHw + ",codeT:" + ginfo.codecType +
                        ", PlayState:" + ginfo.playState + ",h:" + ginfo.height + ",w:" +
                        ginfo.width + ",mCurLayes:" + mCurLayers);
            if (nPlayingHw != 1 ||
                    nlayers > DEFAULT_MAX_LAYERS ||
                    ginfo.codecType != DECODE_HW ||
                    ginfo.playState != VIDEO_PLAYING) {

                boostuperf_set_mode(BOOST_UPERF_NORMAL);
                if (mBoostUPerfTask == null){
                    mBoostUPerfTask = new BoostUPerfTask();
                }
                mTimer.schedule(mBoostUPerfTask, 50, 3*1000);
                mBoostUPerfWatchDog.cancel();
                mBoostUPerfWatchDog = null;
                Is_localvideo = false;
                return;
            }

            if (nlayers < mCurLayers) {
                if (nlayers == 1) {
                    if (ginfo.codecType == DECODE_HW && ginfo.playState == VIDEO_PLAYING) {
                        if (ginfo.height <= 1080 && ginfo.width <= 1920) {
                            boostuperf_set_mode(BOOST_UPERF_LOCALVIDEO);
                        } else {
                            boostuperf_set_mode(BOOST_UPERF_4KLOCALVIDEO);
                        }
                        Is_localvideo = true;
                        mCurLayers = nlayers;
                    }
                }
            }
            return;
        }
    }

    class BoostUPerfTask extends TimerTask {
        public void run() {
            //do somthing
            if (DEBUG)
                Log.e(TAG, "boostUPerfTask-------------------");
            int num = MediaPlayer.getMediaPlayerList();
            int nPlayingHw = 0;

            if (num <= 0)
                return;

            for (int i = 0; i < num; i++) {
                int ret = MediaPlayer.getMediaPlayerInfo(i,minfo);
                if (ret == 0 && minfo.codecType == DECODE_HW && minfo.playState == VIDEO_PLAYING) {
                    nPlayingHw++;
                    ginfo = minfo;
                }
            }

            /*
             * now just one MediaPlayer playing vedio,
             * we check whether it is full-screen
             */

            if (nPlayingHw == 1) {
                int nlayers = SystemProperties.getInt("sys.boost_up_perf.layers", DEFAULT_MAX_LAYERS + 1);
                if (nlayers > DEFAULT_MAX_LAYERS){
                    return;
                }

                if (DEBUG)
                    Log.e(TAG, "boostuperfTask-:" + nlayers + ",codeT:" + ginfo.codecType +
                            ", PlayS:" + ginfo.playState + ",h:" + ginfo.height + ",w:" +
                            ginfo.width + ",mCurLayes:" + mCurLayers);

                if (nlayers == 1) {
                    if (ginfo.codecType == DECODE_HW && ginfo.playState == VIDEO_PLAYING) {
                        if (ginfo.height <= 1080 && ginfo.width <= 1920) {
                            boostuperf_set_mode(BOOST_UPERF_LOCALVIDEO);
                        } else {
                            boostuperf_set_mode(BOOST_UPERF_4KLOCALVIDEO);
                        }
                        Is_localvideo = true;
                    }
                }else if(nlayers == 2) {
                    if (ginfo.codecType == DECODE_HW && ginfo.playState == VIDEO_PLAYING) {
                        if (ginfo.height <= 1080 && ginfo.width <= 1920) {
                            boostuperf_set_mode(BOOST_UPERF_4KLOCALVIDEO);
                            Is_localvideo = true;
                        }
                    }
                }
                /**
                 * Now we are in localvideo mode
                 * Firstly enable watchdog,
                 * Secondly cancel itself, this will be safe, cancel() just set Task.canceld = true,
                 * and the next time schedule will del this Task.
                 */

                if (Is_localvideo) {
                    if (mBoostUPerfWatchDog == null){
                        mBoostUPerfWatchDog = new BoostUPerfWatchDog();
                    }
                    mTimer.schedule(mBoostUPerfWatchDog, 50, 200);
                    mBoostUPerfTask.cancel();
                    mBoostUPerfTask = null;
                    mCurLayers = nlayers;
                    if (DEBUG)
                        Log.e(TAG, "mCurLayers:" + mCurLayers);
                }
            }
            return;
        }
    }


    class LowMemDevKillProTask extends TimerTask {
        public void run() {
            boolean bMemOpt = SystemProperties.getBoolean("sys.mem.opt", false);
            if (bMemOpt) {
                checkToSetProcessLimit();
                //killBackGround();
            }
        }
    }
    /**
     * Boot completed receiver
     */
    private class BoostUPerfBr extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(Intent.ACTION_BOOT_COMPLETED)) {
                Log.i(TAG, "boot completed");
                /* switch cpufreq governor*/
                String board = SystemProperties.get("ro.board.platform", "octopus");
                setCpufreqGovernor(CPU_GOVERNOR_INTERACTIVE, board);
                setCpuAutoHotplug(true);

                mAudioService = (AudioManager)mContext.getSystemService(Context.AUDIO_SERVICE);
                mActivityManager = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
                mProcessLimitAppList = loadAppListFromFile(APP_DETECT_PROCESS_LIMIT_FILE_NAME);

                mSystemIsReady = true;
                mIsLowMemDev   = mActivityManager.isLowRamDevice();

                if (mTimer == null) {
                    mTimer   = new Timer();
                }
                if (mIsLowMemDev) {
                    mTimer.schedule(new LowMemDevKillProTask(), 5*1000, 2*1000);
                }

                //bootfast disturb
                boolean mBoostUperfVideoEnable = SystemProperties.getBoolean("sys.boost_up_perf.video", true);
                if(mBoostUperfVideoEnable && mBoostUPerfTask == null) {
                    mBoostUPerfTask = new BoostUPerfTask();
                    mTimer.schedule(mBoostUPerfTask, 10*1000, 3*1000);
                }
                boostuperf_set_mode(BOOST_UPERF_NORMAL);
                return;
            }

            if (Intent.ACTION_BOOST_UP_PERF.equals(action) ||
                    Intent.ACTION_SCREEN_ON.equals(action) ||
                    Intent.ACTION_SCREEN_OFF.equals(action)) {

                synchronized(syncVar) {
                    set_boost_up_perf_mode(intent);
                }
            }
        }
    }

    private void set_boost_up_perf_mode(Intent intent) {
        String action = intent.getAction();
        boolean enable_boost_up_perf        = SystemProperties.getBoolean("sys.boost_up_perf.enable", true);
        boolean enable_boost_up_bgmusic     = SystemProperties.getBoolean("sys.boost_up_perf.bgmusic", true);
        boolean enable_boost_up_home        = SystemProperties.getBoolean("sys.boost_up_perf.home", true);
        boolean enable_boost_up_rotation    = SystemProperties.getBoolean("sys.boost_up_perf.rotation", true);
        boolean enable_boost_up_extreme     = SystemProperties.getBoolean("sys.boost_up_perf.extreme", true);
        int numDisplays                     = SystemProperties.getInt("sys.boost_up_perf.displays", 1);

        DEBUG = SystemProperties.getBoolean("debug.boost_up_perf.enable", false);

        if (DEBUG)
            Log.e(TAG, "mCurMode:" + mCurMode + ",senctrl_enable:" + mSenctrl_enable + ",index:" + mCurSence_index);

        if (enable_boost_up_perf == false) {
            return;
        }

        if (mSystemIsReady == true) {
            String mode = null;
            int pid   = intent.getIntExtra("pid", 0);
            int index = intent.getIntExtra("index", mCurSence_index);

            if (Intent.ACTION_SCREEN_OFF.equals(action)) {
                mode = BOOST_UPERF_NORMAL;
                if (mAudioService.isMusicActive() && enable_boost_up_bgmusic == true && numDisplays <= 1) {
                    mode = BOOST_UPERF_BGMUSIC;
                }

                if (DEBUG)
                    Log.e(TAG, "Boost up when screen off set-----: " + mode + ",pid:" +pid);

                SystemProperties.set("sys.boost_up_perf.mode", mode + " " + Integer.toString(pid) + " " + Integer.toString(index));

                if (!mCurMode.equals(BOOST_UPERF_EXTREME))
                    mCurMode = mode;
                //this is our last change to set mode, so disable all following intent
                mSenctrl_enable = false;
                return;
            } else if (Intent.ACTION_SCREEN_ON.equals(action)) {
                //if extreme mode, set back
                if (mCurMode.equals(BOOST_UPERF_EXTREME)) {
                    SystemProperties.set("sys.boost_up_perf.mode", mCurMode + " " + Integer.toString(pid) + " " + Integer.toString(index));
                } else {
                    mCurMode = BOOST_UPERF_NORMAL;
                    SystemProperties.set("sys.boost_up_perf.mode", mCurMode + " " + Integer.toString(pid) + " " + Integer.toString(index));
                }

                if (DEBUG)
                    Log.e(TAG, "Boost up when screen on  set-----: " + mCurMode + ",pid:" +pid);
                mSenctrl_enable = true;
                return;
            } else if (Intent.ACTION_BOOST_UP_PERF.equals(action)) {
                mode = intent.getStringExtra("mode");

                if (mode == null) {
                    mode = BOOST_UPERF_NORMAL;
                }

                /**
                 * * Maybe here use switch-case synatx to be a good choice,
                 * * but this can make clear the logic;
                 * * do nothing in these cases,
                 * * 1. screen off
                 * * 2. disable home mode
                 * * 3. disable rotation mode
                 * * 4. disable extreme mode
                 * * 5. current in extreme mode and next mode is not normal
                 * * 6. current in rotation mode or isMusicActive and the Delayed home mode coming
                 * * 7. numDisplays > 1 so home/4klocalvideo/localvideo/ mode disable
                 * */

                //extreme
                if ((BOOST_UPERF_EXTREME.equals(mCurMode) && !mode.equals(BOOST_UPERF_NORMAL)) || !mSenctrl_enable) {
                    return;
                }

                if (mode.equals(BOOST_UPERF_NORMAL)) {
                    if (BOOST_UPERF_EXTREME.equals(mCurMode) && mIsLowMemDev) {
                        LowMemoryDevExitBenMark();
                    }

                } else if (mode.equals(BOOST_UPERF_HOMENTER)) {
                    if (!enable_boost_up_home || numDisplays > 1 ||
                            mAudioService.isMusicActive() || BOOST_UPERF_ROTATENTER.equals(mCurMode)) {
                        return;
                    }
                    /*
                    if (!isApkOnTop(LAUNCHER)) {
                        return;
                    }
                    */

                } else if (mode.equals(BOOST_UPERF_HOMEXIT)) {
                    mode = BOOST_UPERF_NORMAL;

                    if (!enable_boost_up_home) {
                        return;
                    }

                } else if (mode.equals(BOOST_UPERF_ROTATENTER)) {

                    if (!enable_boost_up_rotation) {
                        return;
                    }
                    /*
                    if (isApkOnTop(LAUNCHER)) {
                        if (LauncherPid > 0) {
                            pid = LauncherPid;
                        } else {
                            int lpid = GetPidByname(LAUNCHER);
                            if (lpid > 0) {
                                pid = LauncherPid = lpid;
                            }
                        }
                    }
                    */

                } else if (mode.equals(BOOST_UPERF_ROTATEXIT)) {
                    mode = BOOST_UPERF_NORMAL;

                    if (!enable_boost_up_rotation) {
                        return;
                    }

                } else if (mode.equals(BOOST_UPERF_EXTREME)) {

                    if (!enable_boost_up_extreme) {
                        return;
                    }

                    if (index == 0) {
                        index = mCurSence_index;
                    } else {
                        mCurSence_index = index;
                    }

                    if (mIsLowMemDev) {
                        LowMemoryDevEnterBenMark();
                    }

                } else if (mode.equals(BOOST_UPERF_LOCALVIDEO)) {
                    if (numDisplays > 1) {
                        return;
                    }

                } else if (mode.equals(BOOST_UPERF_4KLOCALVIDEO)) {
                    if (numDisplays > 1) {
                        return;
                    }
                }

                if (mCurMode.equals(mode)) {
                    return;
                }

                if (DEBUG)
                    Log.e(TAG, "Boost up on demand set-----: " + mode + ",pid:" + pid + ",index:" + index);

                SystemProperties.set("sys.boost_up_perf.mode", mode + " " + Integer.toString(pid) + " " + Integer.toString(index));
                mCurMode = mode;
                return;
            }
        }
    }

    private boolean writeFilex(String path, String data) {
        FileOutputStream fos = null;
        boolean nRet = true;

        try {
            fos = new FileOutputStream(path);
            fos.write(data.getBytes("US-ASCII"));
        } catch (IOException e) {
            Log.e(TAG, "Unable to write " + path + e.getMessage());
            e.printStackTrace();
            nRet = false;
        } finally {
            if (fos != null) {
                try {
                    fos.close();
                } catch (IOException e) {
                    nRet = false;
                }
            }
        }
        return nRet;
    }

    private String readFileByLines(String fileName) {
        File file = new File(fileName);
        BufferedReader reader = null;
        String tempString = null;

        try {
            reader = new BufferedReader(new FileReader(file));
            // read first line
            tempString = reader.readLine();
            if (DEBUG)
                Log.d(TAG, "-----------line1 " + ": " + tempString);
            reader.close();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e1) {
                }
            }
        }
        return tempString;
    }

    private boolean setCpufreqGovernor(String governor, String board) {
        if (board.equals("kylin")) {
            writeFilex(CPU0BOOT_LOCK_FILE, "0\n");
            writeFilex(CPU0FREQ_GOVERNOR_PATH, governor);
            writeFilex(CPU4BOOT_LOCK_FILE, "0\n");
            writeFilex(CPU4FREQ_GOVERNOR_PATH, governor);
        } else if (board.equals("octopus") || board.equals("astar")) {
            writeFilex(CPU0BOOT_LOCK_FILE, "0\n");
            writeFilex(CPU0FREQ_GOVERNOR_PATH, governor);
        } else {
            writeFilex(CPU0BOOT_LOCK_FILE, "0\n");
            writeFilex(CPU0FREQ_GOVERNOR_PATH, governor);
        }
        return true;
    }

    private boolean setCpuAutoHotplug(boolean enable) {
        boolean ret = true;
        if (enable) {
            ret =  writeFilex(CPU_AUTO_HOTPLUG_FILE, "1");
        } else {
            ret = writeFilex(CPU_AUTO_HOTPLUG_FILE, "0");
        }
        Log.e(TAG, "setcpuAutuHotPlug-----------ret:" + ret);
        return ret;
    }

    public void systemReady() {
    }

    private void LowMemoryDevEnterBenMark() {
        boolean bMemOpt = SystemProperties.getBoolean("sys.mem.opt", false);

        //in benchmark
        if (bMemOpt) {
            //checkToSetProcessLimit();
            //killBackGround();

            if (DEBUG)
                Log.d(TAG,"++++++++++++++++ lowmemorykiller set +++++++++");
            mAdj = readFileByLines(LOWMEM_ADJ_PATH);
            mMinfree = readFileByLines(LOWMEM_MINFREE_PATH);
            writeFilex(LOWMEM_ADJ_PATH, "0");
            writeFilex(LOWMEM_MINFREE_PATH, "0");
        }
    }

    private void LowMemoryDevExitBenMark() {
       // out benchmark
       boolean bMemOpt = SystemProperties.getBoolean("sys.mem.opt", false);
       if (bMemOpt) {
           //checkToSetProcessLimit();
           if (DEBUG)
               Log.d(TAG,"++++++++++++++++ lowmemorykiller restore +++++++++");
           if (mAdj != null)
               writeFilex(LOWMEM_ADJ_PATH, mAdj);
           if (mMinfree != null)
               writeFilex(LOWMEM_MINFREE_PATH, mMinfree);
       }
    }

    private ArrayList<String> loadAppListFromFile(String strFile) {
        ArrayList<String> strAppList = new ArrayList<String>();
        try {
            mOriginalLimit = ActivityManagerNative.getDefault().getProcessLimit();
        } catch (Exception e){

        }

        try {
            BufferedReader br = new BufferedReader(new InputStreamReader(mContext.getAssets().open(strFile)));
            String tmpStr = null;

            while ((tmpStr = br.readLine()) != null) {
                strAppList.add(tmpStr);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        return strAppList;
    }

    private void killBackGround(){
        List<ActivityManager.RunningAppProcessInfo> list = mActivityManager.getRunningAppProcesses();
        if (list!=null) {
            for (int i = 0; i < list.size(); i++) {
                ActivityManager.RunningAppProcessInfo apinfo=list.get(i);
                if (DEBUG) {
                    Log.i(TAG, "pid:"+ apinfo.pid + "processName:"+ apinfo.processName + "importance:"+ apinfo.importance);
                }

                String[] pkgList=apinfo.pkgList;
                if ((false == apinfo.processName.contains("com.antutu.ABenchMark")) && apinfo.importance > ActivityManager.RunningAppProcessInfo.IMPORTANCE_SERVICE) {
                    for (int j=0; j < pkgList.length; j++) {
                        final String psName = apinfo.processName;
                        if (DEBUG)
                            Log.i(TAG, "kill app:"+apinfo.processName);
                        mActivityManager.killBackgroundProcesses(apinfo.processName);
                    }
                }
            }
        }
    }

    private void checkToSetProcessLimit() {
        ComponentName componentName = mActivityManager.getRunningTasks(1).get(0).topActivity;
        String packageName = componentName.getPackageName();

        try {
            int nLimit = ActivityManagerNative.getDefault().getProcessLimit();
            if (mProcessLimitAppList.contains(packageName)) {
                if (nLimit == -1 || nLimit >  PROCESS_LIMITATION_NUM) {
                    mOriginalLimit = nLimit;
                    ActivityManagerNative.getDefault().setProcessLimit(PROCESS_LIMITATION_NUM);
                }
            } else {
                if (mOriginalLimit != PROCESS_LIMITATION_NUM && mOriginalLimit != nLimit) {
                    ActivityManagerNative.getDefault().setProcessLimit(mOriginalLimit);
                    mOriginalLimit = -1;
                }
            }
        } catch (Exception e) {

        }
    }

    private boolean isApkOnTop(String name) {
        ComponentName componentName = mActivityManager.getRunningTasks(1).get(0).topActivity;
        String packageName = componentName.getPackageName();
        if (name.compareTo(packageName) == 0) {
            return true;
        }
        return false;
    }

    private int GetPidByname(String name) {
        List<ActivityManager.RunningAppProcessInfo> list = mActivityManager.getRunningAppProcesses();
        if (list!=null) {
            for (int i = 0; i < list.size(); i++) {
                ActivityManager.RunningAppProcessInfo apinfo = list.get(i);
                if (DEBUG) {
                    Log.i(TAG, "pid:"+ apinfo.pid + "processName:"+ apinfo.processName + "importance:"+ apinfo.importance);
                }
                if (apinfo.processName.equals(name))
                    return apinfo.pid;
            }
        }
        return 0;
    }

    public void notifyDPM(Intent intent){
        synchronized(syncVar) {
            if (DEBUG)
                Log.e(TAG,"notifyDPM---------------mode:" + intent.getStringExtra("mode"));
            set_boost_up_perf_mode(intent);
        }
       }
}
