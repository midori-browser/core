package org.midori_browser.midori

import android.os.Message
import android.support.v7.widget.Toolbar
import android.webkit.GeolocationPermissions
import android.webkit.WebChromeClient
import android.webkit.WebView
import kotlinx.android.synthetic.main.activity_browser.*

class WebChromeClient constructor(val activity: BrowserActivity) : WebChromeClient() {
    override fun onProgressChanged(view: WebView?, newProgress: Int) {
        activity.loadingProgress.progress = newProgress * 1000
    }

    override fun onReceivedTitle(view: WebView?, title: String?) {
        val toolbar = activity.findViewById<Toolbar>(R.id.toolbar)
        toolbar.title = title
    }
}
