/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the Hightail Bigfile backend.
 */

"use strict";

var MODULE_NAME = 'test-cloudfile-backend-hightail';

var RELATIVE_ROOT = '../shared-modules';
var MODULE_REQUIRES = ['folder-display-helpers',
                         'compose-helpers',
                         'cloudfile-helpers',
                         'cloudfile-backend-helpers',
                         'cloudfile-hightail-helpers',
                         'observer-helpers',
                         'prompt-helpers',];

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var gServer, gObsManager;

function setupModule(module) {
  for (let lib of MODULE_REQUIRES) {
    collector.getModule(lib).installInto(module);
  }

  gObsManager = new SimpleRequestObserverManager();

  // Enable logging for this group of tests.
  Services.prefs.setCharPref("Hightail.logging.dump", "All");
}

function teardownModule(module) {
  Services.prefs.clearUserPref("Hightail.logging.dump");
}

function setupTest() {
  gServer = new MockHightailServer();
  gServer.init();
  gServer.start();
}

function teardownTest() {
  Services.prefs.QueryInterface(Ci.nsIPrefBranch)
          .deleteBranch("mail.cloud_files.accounts");
  gObsManager.check();
  gObsManager.reset();
  gServer.stop(mc);
}

function test_simple_case() {
  const kExpectedUrl = "http://www.example.com/expectedUrl";
  const kTopics = [kUploadFile, kGetFileURL];

  gServer.planForUploadFile("testFile1");
  gServer.planForGetFileURL("testFile1", {url: kExpectedUrl});

  let obs = new ObservationRecorder();
  for (let topic of kTopics) {
    obs.planFor(topic);
    Services.obs.addObserver(obs, topic);
  }

  let requestObserver = gObsManager.create("test_simple_case - Upload 1");
  let file = getFile("./data/testFile1", __file__);
  let provider = gServer.getPreparedBackend("someAccountKey");
  provider.uploadFile(file, requestObserver);

  mc.waitFor( () => requestObserver.success);

  let urlForFile = provider.urlForFile(file);
  assert_equals(kExpectedUrl, urlForFile);
  assert_equals(1, obs.numSightings(kUploadFile));
  assert_equals(1, obs.numSightings(kGetFileURL));

  gServer.planForUploadFile("testFile1");
  gServer.planForGetFileURL("testFile1", {url: kExpectedUrl});
  requestObserver = gObsManager.create("test_simple_case - Upload 2");
  provider.uploadFile(file, requestObserver);
  mc.waitFor(() => requestObserver.success);
  urlForFile = provider.urlForFile(file);
  assert_equals(kExpectedUrl, urlForFile);

  assert_equals(2, obs.numSightings(kUploadFile));
  assert_equals(2, obs.numSightings(kGetFileURL));

  for (let topic of kTopics) {
    Services.obs.removeObserver(obs, topic);
  }
}

function test_chained_uploads() {
  const kExpectedUrlRoot = "http://www.example.com/";
  const kTopics = [kUploadFile, kGetFileURL];
  const kFilenames = ["testFile1", "testFile2", "testFile3"];

  for (let filename of kFilenames) {
    let expectedUrl = kExpectedUrlRoot + filename;
    gServer.planForUploadFile(filename);
    gServer.planForGetFileURL(filename, {url: expectedUrl});
  }

  let obs = new ObservationRecorder();
  for (let topic of kTopics) {
    obs.planFor(topic);
    Services.obs.addObserver(obs, topic);
  }

  let provider = gServer.getPreparedBackend("someAccountKey");

  let files = [];

  let observers = kFilenames.map(function(aFilename) {
    let requestObserver = gObsManager.create("test_chained_uploads for filename " + aFilename);
    let file = getFile("./data/" + aFilename, __file__);
    files.push(file);
    provider.uploadFile(file, requestObserver);
    return requestObserver;
  });

  mc.waitFor(function() {
    return observers.every(aListener => aListener.success);
  }, "Timed out waiting for chained uploads to complete.", 10000);

  assert_equals(kFilenames.length, obs.numSightings(kUploadFile));

  for (let [index, filename] of kFilenames.entries()) {
    assert_equals(obs.data[kUploadFile][index], filename);
    let file = getFile("./data/" + filename, __file__);
    let expectedUriForFile = kExpectedUrlRoot + filename;
    let uriForFile = provider.urlForFile(files[index]);
    assert_equals(expectedUriForFile, uriForFile);
  }

  assert_equals(kFilenames.length, obs.numSightings(kGetFileURL));

  for (let topic of kTopics) {
    Services.obs.removeObserver(obs, topic);
  }
}

function test_deleting_uploads() {
  const kFilename = "testFile1";
  let provider = gServer.getPreparedBackend("someAccountKey");
  // Upload a file

  let file = getFile("./data/" + kFilename, __file__);
  gServer.planForUploadFile(kFilename);
  let requestObserver = gObsManager.create("test_deleting_uploads - upload 1");
  provider.uploadFile(file, requestObserver);
  mc.waitFor(() => requestObserver.success);

  // Try deleting a file
  let obs = new ObservationRecorder();
  obs.planFor(kDeleteFile)
  Services.obs.addObserver(obs, kDeleteFile);

  gServer.planForDeleteFile(kFilename);
  let deleteObserver = gObsManager.create("test_deleting_uploads - delete 1");
  provider.deleteFile(file, deleteObserver);
  mc.waitFor(() => deleteObserver.success);

  // Check to make sure the file was deleted on the server
  assert_equals(1, obs.numSightings(kDeleteFile));
  assert_equals(obs.data[kDeleteFile][0], kFilename);
  Services.obs.removeObserver(obs, kDeleteFile);
}

/**
 * Test that cancelling an upload causes onStopRequest to be
 * called with nsIMsgCloudFileProvider.uploadCanceled.
 */
function test_can_cancel_upload() {
  const kFilename = "testFile1";
  let provider = gServer.getPreparedBackend("anAccount");
  let file = getFile("./data/" + kFilename, __file__);
  gServer.planForUploadFile(kFilename, 2000);
  assert_can_cancel_uploads(mc, provider, [file]);
}

/**
 * Test that cancelling several uploads causes onStopRequest to be
 * called with nsIMsgCloudFileProvider.uploadCanceled.
 */
function test_can_cancel_uploads() {
  const kFiles = ["testFile2", "testFile3", "testFile4"];
  let provider = gServer.getPreparedBackend("anAccount");
  let files = [];
  for (let filename of kFiles) {
    gServer.planForUploadFile(filename, 2000);
    files.push(getFile("./data/" + filename, __file__));
  }
  assert_can_cancel_uploads(mc, provider, files);
}

/**
 * Test that when we call createExistingAccount, onStopRequest is successfully
 * called, and we pass the correct parameters.
 */
function test_create_existing_account() {
  // We have to mock out the auth prompter, or else we'll lock up.
  gMockAuthPromptReg.register();
  let provider = gServer.getPreparedBackend("someNewAccount");
  let done = false;
  let myObs = {
    onStartRequest: function(aRequest, aContext) {
    },
    onStopRequest: function(aRequest, aContext, aStatusCode) {
      assert_true(aContext instanceof Ci.nsIMsgCloudFileProvider);
      assert_equals(aStatusCode, Cr.NS_OK);
      done = true;
    },
  }

  provider.createExistingAccount(myObs);
  mc.waitFor(() => done);
  gMockAuthPromptReg.unregister();
}

function test_delete_refreshes_stale_token() {
  const kFilename = "testFile1";
  const kUserEmail = "test@example.com";

  // Stop the default Hightail mock server, and create our own.
  gServer.stop(mc);
  gServer = new MockHightailServer();
  // Replace the auth component with one that counts auth
  // requests.
  gServer.auth = new MockHightailAuthCounter(gServer);
  // Replace the deleter component with one that returns the
  // stale token error.
  gServer.deleter = new MockHightailDeleterStaleToken(gServer);
  // Fire up the server.
  gServer.init();
  gServer.start();

  gServer.setupUser({email: kUserEmail});

  // We're testing to make sure that we refresh on stale tokens
  // iff a password has been remembered for the user, so make
  // sure we remember a password...
  remember_hightail_credentials(kUserEmail, "somePassword");

  // Get a prepared provider...
  let provider = gServer.getPreparedBackend("someAccountKey");
  let file = getFile("./data/" + kFilename, __file__);
  gServer.planForUploadFile(kFilename);
  let requestObserver = gObsManager.create("test_delete_refreshes_stale_token - upload 1");
  provider.uploadFile(file, requestObserver);
  mc.waitFor(() => requestObserver.success);

  // At this point, we should not have seen any auth attempts.
  assert_equals(gServer.auth.count, 0,
                "Should not have seen any authorization attempts "
                + "yet");

  // Now delete the file, and get the stale auth token warning.
  gServer.planForDeleteFile(kFilename);
  // We have to pass an observer, so we'll just generate a dummy one.
  let deleteObserver = new SimpleRequestObserver();
  provider.deleteFile(file, deleteObserver);

  // Now, since the token was stale, we should see us hit authentication
  // again.
  mc.waitFor(() => gServer.auth.count > 0,
             "Timed out waiting for authorization attempt");
}

/**
 * Test for bug 771132 - if Hightail returns malformed URLs, prefix with
 * https://.
 */
function test_bug771132_fix_no_scheme() {
  const kMalformedUrl = "www.example.com/download/abcd1234";
  const kExpectedUrl = "https://" + kMalformedUrl;

  const kTopics = [kUploadFile, kGetFileURL];

  gServer.planForUploadFile("testFile1");
  gServer.planForGetFileURL("testFile1", {url: kMalformedUrl});

  let requestObserver = gObsManager.create("test_simple_case - Upload 1");
  let file = getFile("./data/testFile1", __file__);
  let provider = gServer.getPreparedBackend("someAccountKey");
  provider.uploadFile(file, requestObserver);

  mc.waitFor(() => requestObserver.success);

  let urlForFile = provider.urlForFile(file);
  assert_equals(kExpectedUrl, urlForFile);

  // Now make sure that we don't accidentally prefix with https when there's
  // already a scheme.
  gServer.planForUploadFile("testFile1");
  gServer.planForGetFileURL("testFile1", {url: kExpectedUrl});
  requestObserver = gObsManager.create("test_simple_case - Upload 2");
  provider.uploadFile(file, requestObserver);
  mc.waitFor(() => requestObserver.success);

  urlForFile = provider.urlForFile(file);
  assert_equals(kExpectedUrl, urlForFile);
}
