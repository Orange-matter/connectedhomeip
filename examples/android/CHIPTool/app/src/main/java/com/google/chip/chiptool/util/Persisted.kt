package com.google.chip.chiptool.util

import android.content.Context


class Persisted(context: Context) {
    // WARNING : must use same shared prefs that the one used by controller lib, see PreferencesKeyValueStoreManager
    private val sharedPref by lazy { context.getSharedPreferences("chip.platform.KeyValueStore", Context.MODE_PRIVATE) }

    val fabricId = LongPref(sharedPref, FABRIC_ID_KEY)
    val nodeId = LongPref(sharedPref, NODE_ID_KEY)
    val rootCert = StringPref(sharedPref, ROOT_CERT_KEY)
    val rootKey = StringPref(sharedPref, ROOT_KEY_PAIR_KEY)
    val intermediateCert = StringPref(sharedPref, INTERMEDIATE_CERT_KEY)
    val intermediateKey = StringPref(sharedPref, INTERMEDIATE_KEY_PAIR_KEY)

    companion object {
        private const val FABRIC_ID_KEY = "fabric-id"
        private const val NODE_ID_KEY = "node-id"
        private const val ROOT_CERT_KEY = "AndroidCARootCert0"
        private const val ROOT_KEY_PAIR_KEY = "AndroidCARootKey"
        private const val INTERMEDIATE_CERT_KEY = "AndroidCAIntermediateCert0"
        private const val INTERMEDIATE_KEY_PAIR_KEY = "AndroidCAIntermediateKey"
    }
}
