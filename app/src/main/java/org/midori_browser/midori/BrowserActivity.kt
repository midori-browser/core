package org.midori_browser.midori

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.support.annotation.RequiresApi
import android.support.v7.app.AppCompatActivity
import android.text.Editable
import android.text.TextWatcher
import android.view.KeyEvent
import android.view.Menu
import android.view.MenuItem
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.webkit.CookieManager
import android.webkit.WebSettings
import android.webkit.WebStorage
import android.widget.AdapterView
import android.widget.ArrayAdapter
import kotlinx.android.synthetic.main.activity_browser.*

class BrowserActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_browser)
        setSupportActionBar(findViewById(R.id.toolbar))

        val webSettings = webView.settings
        webSettings.javaScriptEnabled = true
        requestDesktopSite(false)
        webSettings.databaseEnabled = true
        webSettings.setAppCacheEnabled(true)
        webSettings.domStorageEnabled = true
        @RequiresApi(Build.VERSION_CODES.LOLLIPOP)
        webSettings.mixedContentMode = WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE
        webView.webViewClient = WebViewClient(this)
        webView.webChromeClient = WebChromeClient(this)


        val openTabs = (getSharedPreferences("config", Context.MODE_PRIVATE).getString(
            "openTabs", null
        ) ?: getString(R.string.appWebsite)).split(",".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()
        webView.loadUrl(openTabs.first())

        val adapter = ArrayAdapter<String>(
            this,
            android.R.layout.simple_spinner_dropdown_item, completion
        )
        urlBar.setAdapter(adapter)
        urlBar.onItemClickListener = AdapterView.OnItemClickListener { parent, _, position, _ ->
            val im = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            im.hideSoftInputFromWindow(parent.applicationWindowToken, 0)
            loadUrlOrSearch(parent.getItemAtPosition(position).toString())
        }
        urlBar.setOnEditorActionListener() { v, actionId, event ->
            if ((event != null && event.keyCode == KeyEvent.KEYCODE_ENTER) || actionId == EditorInfo.IME_ACTION_DONE) {
                val im = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                im.hideSoftInputFromWindow(v.applicationWindowToken, 0)
                loadUrlOrSearch(urlBar.text.toString())
                true
            } else {
                false
            }
        }
        urlBar.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}

            override fun afterTextChanged(s: Editable?) {
            }
        })
    }

    private fun loadUrlOrSearch(text: String) {
        webView.loadUrl(magicUri(text) ?: uriForSearch(text))
    }

    fun magicUri(text: String): String? {
        if (" " in text) {
            return null
        } else if (text.startsWith("http")) {
            return text
        } else if (text == "localhost" || "." in text) {
            return "http://" + text
        }
        return null
    }

    val locationEntrySearch = "https://duckduckgo.com/?q=%s"
    fun uriForSearch(keywords: String? = null, search: String? = null): String {
        val uri = search ?: locationEntrySearch
        val escaped = Uri.encode(keywords ?: "", ":/")
        // Allow DuckDuckGo to distinguish Midori and in turn share revenue
        if (uri == "https://duckduckgo.com/?q=%s") {
            return "https://duckduckgo.com/?q=$escaped&t=midori"
        } else if ("%s" in uri) {
            return uri.format(escaped)
        }
        return uri + escaped

    }

    val completion = listOf("www.midori-browser.org", "example.com", "duckduckgo.com")
    fun requestDesktopSite(desktopSite: Boolean) {
        webView.settings.apply {
            userAgentString = null // Reset to default
            userAgentString = userAgentString + " " + getString(R.string.userAgentVersion)
            if (desktopSite) {
                // Websites look for "Android" and "Mobile" keywords to decide what's not desktop
                val mobileOS = userAgentString.substring(userAgentString.indexOf("("), userAgentString.indexOf(")") + 1)
                userAgentString = userAgentString.replace(mobileOS, "(X11; Linux x86_64)").replace(" Mobile", "")
            }
            useWideViewPort = desktopSite
            loadWithOverviewMode = desktopSite
        }
    }


    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.app_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem) = when (item.itemId) {
        R.id.actionShare -> {
            val intent = Intent(Intent.ACTION_SEND)
            val uri = Uri.parse(webView.url)
            intent.putExtra(Intent.EXTRA_TEXT, uri.toString())
            intent.type = "text/plain"
            startActivity(Intent.createChooser(intent, getString(R.string.actionShare)))
            true
        }
        R.id.actionClearPrivateData -> {
            @Suppress("DEPRECATION")
            CookieManager.getInstance().removeAllCookie()
            WebStorage.getInstance().deleteAllData()
            webView.clearCache(true)
            true
        }
        R.id.actionRequestDesktopSite -> {
            item.isChecked = !item.isChecked
            requestDesktopSite(item.isChecked)
            webView.reload()
            true
        }
        else -> {
            super.onOptionsItemSelected(item)
        }
    }
}
