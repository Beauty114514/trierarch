package app.trierarch

import android.app.Application

class TrierarchApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        X11Runtime.initInMainProcess(this)
    }
}
