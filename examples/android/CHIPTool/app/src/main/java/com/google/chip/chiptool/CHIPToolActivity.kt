/*
 *   Copyright (c) 2020 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */
package com.google.chip.chiptool

import android.content.Intent
import android.net.Uri
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.os.Bundle
import android.util.Base64
import android.util.Log
import android.util.Xml
import android.view.Menu
import android.view.MenuItem
import android.widget.EditText
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import com.google.chip.chiptool.NetworkCredentialsParcelable
import chip.setuppayload.SetupPayload
import chip.setuppayload.SetupPayloadParser
import chip.setuppayload.SetupPayloadParser.UnrecognizedQrCodeException
import com.google.chip.chiptool.attestation.AttestationTestFragment
import com.google.chip.chiptool.clusterclient.*
import com.google.chip.chiptool.clusterclient.clusterinteraction.ClusterInteractionFragment
import com.google.chip.chiptool.provisioning.AddressCommissioningFragment
import com.google.chip.chiptool.provisioning.DeviceProvisioningFragment
import com.google.chip.chiptool.provisioning.EnterNetworkFragment
import com.google.chip.chiptool.provisioning.ProvisionNetworkType
import com.google.chip.chiptool.setuppayloadscanner.BarcodeFragment
import com.google.chip.chiptool.setuppayloadscanner.CHIPDeviceDetailsFragment
import com.google.chip.chiptool.setuppayloadscanner.CHIPDeviceInfo
import com.google.chip.chiptool.setuppayloadscanner.CHIPLedgerDetailsFragment
import com.google.chip.chiptool.util.Persisted
import com.google.chip.chiptool.util.StringPref
import org.json.JSONObject
import org.xmlpull.v1.XmlPullParser

class CHIPToolActivity :
  AppCompatActivity(),
  BarcodeFragment.Callback,
  SelectActionFragment.Callback,
  DeviceProvisioningFragment.Callback,
  EnterNetworkFragment.Callback,
  CHIPDeviceDetailsFragment.Callback,
  CHIPLedgerDetailsFragment.Callback {

  private var networkType: ProvisionNetworkType? = null
  private var deviceInfo: CHIPDeviceInfo? = null

  private val persisted by lazy { Persisted(this) }

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.top_activity)

    if (savedInstanceState == null) {
      val fragment = SelectActionFragment.newInstance()
      supportFragmentManager
          .beginTransaction()
          .add(R.id.fragment_container, fragment, fragment.javaClass.simpleName)
          .commit()
    } else {
      networkType =
          ProvisionNetworkType.fromName(savedInstanceState.getString(ARG_PROVISION_NETWORK_TYPE))
    }

    if (intent?.action == NfcAdapter.ACTION_NDEF_DISCOVERED)
      onNfcIntent(intent)

    if (Intent.ACTION_VIEW == intent?.action) {
      onReturnIntent(intent)
    }

    restorePersistentData()
  }

  private fun restorePersistentData() {
    if (persisted.nodeId.isSet) {
      ChipClient.nodeId = persisted.nodeId.get()
    }

    if (persisted.fabricId.isSet) {
      ChipClient.fabricId = persisted.fabricId.get()
    }
  }

  private fun StringPref.init(@StringRes res: Int) { if (!isSet) set(getString(res)) }

  override fun onCreateOptionsMenu(menu: Menu?): Boolean {
    menuInflater.inflate(R.menu.menu, menu)
    return true
  }

  override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
    R.id.node_id -> {
      updateFabricAndNodeId()
      true
    }
    R.id.certs_id -> {
      loadCerts()
      true
    }
    else -> super.onOptionsItemSelected(item)
  }

  private fun updateFabricAndNodeId() {
    val customView = layoutInflater.inflate(R.layout.controller_node_id_dialog, null, false)
    val fabricIdText = customView.findViewById<EditText>(R.id.fabric_id_text)
    val nodeIdText = customView.findViewById<EditText>(R.id.node_id_text)
    persisted.run {
      if (fabricId.isSet) fabricIdText?.setText(fabricId.get().toString())
      if (nodeId.isSet) nodeIdText?.setText(nodeId.get().toString())
    }

    AlertDialog.Builder(this)
            .setTitle("Assign Fabric and controller node Id")
            .setPositiveButton(android.R.string.ok) { _, _ ->
              fabricIdText.text.toString().toLong().let {
                persisted.fabricId.set(it)
                ChipClient.fabricId = it
              }
              nodeIdText.text.toString().toLong().let {
                persisted.nodeId.set(it)
                ChipClient.nodeId = it
              }
            }
            .setNegativeButton(android.R.string.cancel) { _, _ -> }
            .setView(customView)
            .create()
            .show()
  }

  private val openFileForResult = registerForActivityResult(ActivityResultContracts.OpenDocument()) { it ->
    it?.let { uri ->
      contentResolver.openInputStream(uri).use { stream ->
        Xml.newPullParser()?.let { parser ->
          parser.setInput(stream, null)

          val map = parseCertsFile(parser)

          with(persisted) {
            map.run {
              if (save(valueOf = "root_cert", into = rootCert)
                && save(valueOf = "root_key", into = rootKey)
                && save(valueOf = "ica_cert", into = intermediateCert)
                && save(valueOf = "ica_key", into = intermediateKey)
              ) {
                Toast.makeText(this@CHIPToolActivity, "Certs file successfully loaded ", Toast.LENGTH_SHORT).show()
              }
            }
          }

          Log.d("CHIPToolActivity", "Certs file loaded")

          return@registerForActivityResult
        }
      }

      Log.e("CHIPToolActivity", "Cannot load or parse Certs file")
      Toast.makeText(this, "Cannot load or parse: $uri", Toast.LENGTH_SHORT).show()
    }
  }

  private fun parseCertsFile(parser: XmlPullParser): Map<String, String> {
    val map = mutableMapOf<String, String>()

    var event: Int
    do {
      event = parser.next()
      if (event == XmlPullParser.START_TAG && parser.name == "item") {
          map[parser.getAttributeValue(null,"name")] = parser.nextText()
      }
    } while (event != XmlPullParser.END_DOCUMENT)
    return map
  }

  private fun loadCerts() {
    openFileForResult.launch(arrayOf("text/*"))
  }

  private fun Map<String, String>.save(valueOf: String, into: StringPref) = get(valueOf)
    ?.let {
      into.set(it)
      true
    }
    ?: run {
      Toast.makeText(this@CHIPToolActivity, "Cannot find value for $valueOf", Toast.LENGTH_SHORT).show()
      false
    }

  override fun onSaveInstanceState(outState: Bundle) {
    outState.putString(ARG_PROVISION_NETWORK_TYPE, networkType?.name)

    super.onSaveInstanceState(outState)
  }

  override fun onCHIPDeviceInfoReceived(deviceInfo: CHIPDeviceInfo) {
    this.deviceInfo = deviceInfo
    if (networkType == null) {
      showFragment(CHIPDeviceDetailsFragment.newInstance(deviceInfo))
    } else {
      if (deviceInfo.ipAddress != null) {
        showFragment(DeviceProvisioningFragment.newInstance(deviceInfo!!, null))
      } else {
        showFragment(EnterNetworkFragment.newInstance(networkType!!), false)
      }
    }
  }

  override fun onCommissioningComplete(code: Int) {
    runOnUiThread {
      Toast.makeText(
          this,
          getString(R.string.commissioning_completed, code),
          Toast.LENGTH_SHORT).show()
    }
    ChipClient.getDeviceController(this).close()
    showFragment(SelectActionFragment.newInstance(), false)
  }

  override fun handleScanQrCodeClicked() {
    showFragment(BarcodeFragment.newInstance())
  }

  override fun onProvisionWiFiCredentialsClicked() {
    networkType = ProvisionNetworkType.WIFI
    showFragment(BarcodeFragment.newInstance(), false)
  }

  override fun onProvisionThreadCredentialsClicked() {
    networkType = ProvisionNetworkType.THREAD
    showFragment(BarcodeFragment.newInstance(), false)
  }

  override fun onShowDeviceAddressInput() {
    showFragment(AddressCommissioningFragment.newInstance(), false)
  }

  override fun onNetworkCredentialsEntered(networkCredentials: NetworkCredentialsParcelable) {
    showFragment(DeviceProvisioningFragment.newInstance(deviceInfo!!, networkCredentials))
  }

  override fun handleClusterInteractionClicked() {
    showFragment(ClusterInteractionFragment.newInstance())
  }

  override fun handleWildcardClicked() {
    showFragment(WildcardFragment.newInstance())
  }

  override fun handleOnOffClicked() {
    showFragment(OnOffClientFragment.newInstance())
  }

  override fun handleSensorClicked() {
    showFragment(SensorClientFragment.newInstance())
  }

  override fun handleMultiAdminClicked() {
    showFragment(MultiAdminClientFragment.newInstance())
  }

  override fun handleOpCredClicked() {
    showFragment(OpCredClientFragment.newInstance())
  }

  override fun handleBasicClicked() {
    showFragment(BasicClientFragment.newInstance())
  }

  override fun handleAttestationTestClicked() {
    showFragment(AttestationTestFragment.newInstance())
  }

  override fun handleReadFromLedgerClicked(deviceInfo: CHIPDeviceInfo) {
    showFragment(CHIPLedgerDetailsFragment.newInstance(deviceInfo))
  }

  override fun handleCustomFlowRedirectClicked(redirectUrl: String) {
    val redirectIntent = Intent(Intent.ACTION_VIEW, Uri.parse(redirectUrl))
    startActivity(redirectIntent)
  }

  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
    super.onActivityResult(requestCode, resultCode, data)

    if (requestCode == REQUEST_CODE_COMMISSIONING) {
      // Simply ignore the commissioning result.
      // TODO: tracking commissioned devices.
    }
  }

  override fun handleCustomFlowClicked() {
    showFragment(BarcodeFragment.newInstance())
  }

  private fun showFragment(fragment: Fragment, showOnBack: Boolean = true) {
    val fragmentTransaction = supportFragmentManager
        .beginTransaction()
        .replace(R.id.fragment_container, fragment, fragment.javaClass.simpleName)

    if (showOnBack) {
      fragmentTransaction.addToBackStack(null)
    }

    fragmentTransaction.commit()
  }

  private fun onNfcIntent(intent: Intent?) {
    // Require 1 NDEF message containing 1 NDEF record
    val messages = intent?.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
    if (messages?.size != 1) return

    val records = (messages[0] as NdefMessage).records
    if (records.size != 1) return

    // Require NDEF URI record starting with "mt:"
    val uri = records[0].toUri()
    if (!uri?.scheme.equals("mt", true)) return

    lateinit var setupPayload: SetupPayload
    try {
      setupPayload =
        SetupPayloadParser().parseQrCode(uri.toString().toUpperCase())
    } catch (ex: UnrecognizedQrCodeException) {
      Log.e(TAG, "Unrecognized QR Code", ex)
      Toast.makeText(this, "Unrecognized QR Code", Toast.LENGTH_SHORT).show()
      return
    }

    val deviceInfo = CHIPDeviceInfo.fromSetupPayload(setupPayload)

    val buttons = arrayOf(
        getString(R.string.nfc_tag_action_show),
        getString(R.string.nfc_tag_action_wifi),
        getString(R.string.nfc_tag_action_thread))

    AlertDialog.Builder(this)
        .setTitle(R.string.nfc_tag_action_title)
        .setItems(buttons) { _, which ->
          this.networkType = when (which) {
            1 -> ProvisionNetworkType.WIFI
            2 -> ProvisionNetworkType.THREAD
            else -> null
          }
          onCHIPDeviceInfoReceived(deviceInfo)
        }
        .create()
        .show()
  }

  private fun onReturnIntent(intent: Intent) {
    val appLinkData = intent.data
    // Require URI schema "mt:"
    if (!appLinkData?.scheme.equals("mt", true)) {
      Log.d(TAG, "Unrecognized URI schema : ${appLinkData?.scheme}")
      return
    }
    // Require URI host "modelinfo"
    if (!appLinkData?.host.equals("modelinfo", true)) {
      Log.d(TAG, "Unrecognized URI host : ${appLinkData?.host}")
      return
    }

    // parse payload
    try {
      val payloadBase64String = appLinkData?.getQueryParameter("payload")
      if (payloadBase64String.isNullOrEmpty()) {
        Log.d(TAG, "Unrecognized payload")
        return
      }

      val decodeBytes = Base64.decode(payloadBase64String, Base64.DEFAULT)
      val payloadString = String(decodeBytes)
      val payload = JSONObject(payloadString)

      // parse payload from JSON
      val setupPayload = SetupPayload()
      // set defaults
      setupPayload.discoveryCapabilities = setOf()
      setupPayload.optionalQRCodeInfo = mapOf()

      // read from payload
      setupPayload.version = payload.getInt("version")
      setupPayload.vendorId = payload.getInt("vendorId")
      setupPayload.productId = payload.getInt("productId")
      setupPayload.commissioningFlow = payload.getInt("commissioningFlow")
      setupPayload.discriminator = payload.getInt("discriminator")
      setupPayload.setupPinCode = payload.getLong("setupPinCode")

      val deviceInfo = CHIPDeviceInfo.fromSetupPayload(setupPayload)
      val buttons = arrayOf(
        getString(R.string.nfc_tag_action_show)
      )

      AlertDialog.Builder(this)
        .setTitle(R.string.provision_custom_flow_alert_title)
        .setItems(buttons) { _, _ ->
          onCHIPDeviceInfoReceived(deviceInfo)
        }
        .create()
        .show()

    } catch (ex: UnrecognizedQrCodeException) {
      Log.e(TAG, "Unrecognized Payload", ex)
      Toast.makeText(this, "Unrecognized Setup Payload", Toast.LENGTH_SHORT).show()
      return
    }
  }

  companion object {
    private const val TAG = "CHIPToolActivity"
    private const val ADDRESS_COMMISSIONING_FRAGMENT_TAG = "address_commissioning_fragment"
    private const val ARG_PROVISION_NETWORK_TYPE = "provision_network_type"

    var REQUEST_CODE_COMMISSIONING = 0xB003
  }
}
