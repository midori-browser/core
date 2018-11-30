package org.midori_browser.midori

import android.content.Intent
import android.net.Uri
import android.webkit.WebView
import android.webkit.WebViewClient
import kotlinx.android.synthetic.main.activity_browser.*

class WebViewClient(val activity: BrowserActivity) : WebViewClient() {

    override fun shouldOverrideUrlLoading(view: WebView?, url: String?): Boolean {
        if (url != null && url.startsWith("http")) {
            activity.urlBar.setText(url)
            return false
        }

        activity.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
        return true
    }
}
