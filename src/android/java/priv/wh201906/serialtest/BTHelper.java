package priv.wh201906.serialtest;

import android.bluetooth.*;
import android.view.WindowManager;
import android.app.Activity;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.lang.String;

public class BTHelper {
    public String TestStr() {
        return "Hello";
    }

    public String[] getBondedDevices() {
        BluetoothAdapter BTAdapter = BluetoothAdapter.getDefaultAdapter();
        Set<BluetoothDevice> bondedDevice = BTAdapter.getBondedDevices();
        ArrayList<String> list = new ArrayList<String>();
        for (BluetoothDevice bt : bondedDevice) {
            list.add(bt.getAddress() + " " + bt.getName());
        }
        String[] result = (String[]) list.toArray(new String[list.size()]);
        return result;
    }

    public void keepScreenOn(Activity activity) {
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

}
