# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

<vbox id="editcard">
  <tabbox>
    <tabs id="abTabs">
      <tab id="contactTabButton" label="&Contact.tab;"
           accesskey="&Contact.accesskey;"/>
      <tab id="homeTabButton" label="&Home.tab;" accesskey="&Home.accesskey;"/>
      <tab id="workTabButton" label="&Work.tab;" accesskey="&Work.accesskey;"/>
      <tab id="otherTabButton" label="&Other.tab;" accesskey="&Other.accesskey;"/>
      <tab id="chatTabButton" label="&Chat.tab;" accesskey="&Chat.accesskey;"/>
      <tab id="photoTabButton" label="&Photo.tab;" accesskey="&Photo.accesskey;"/>
    </tabs>

    <tabpanels id="abTabPanels" flex="1">
      <!-- ** Name Tab ** -->
      <!-- The following vbox contains two hboxes
           top: Name/Email/Phonenumber bottom: Email prefs. -->
      <vbox id="abNameTab" >
        <!-- This hbox contains two vboxes
             left: Name/Email, right: Phonenumbers -->
        <hbox>
          <vbox id="namesAndEmailAddresses"> <!-- This box contains the Names and Emailnames -->

            <!-- LOCALIZATION NOTE:
                 NameField1, NameField2, PhoneticField1, PhoneticField2
                 those fields are either LN or FN depends on the target country.
                 They are configurable in the .dtd file.
              -->

            <hbox id="NameField1Container" align="center">
              <spacer flex="1"/>
              <label control="&NameField1.id;" value="&NameField1.label;"
                     accesskey="&NameField1.accesskey;"/>
              <hbox class="CardEditWidth" align="center">
                <textbox id="&NameField1.id;" flex="1"
                         oninput="GenerateDisplayName()"/>

                <!-- LOCALIZATION NOTE:
                     Fields for phonetic are disabled as default and can be
                     enabled by^editing "mail.addr_book.show_phonetic_fields"
                  -->

                <spacer id="PhoneticSpacer1" flex="1" hidden="true"/>
                <label id="PhoneticLabel1" control="&PhoneticField1.id;"
                       value="&PhoneticField1.label;" hidden="true"/>
                <textbox id="&PhoneticField1.id;" flex="1" hidden="true"/>
              </hbox>
            </hbox>
            <hbox id="NameField2Container" align="center">
              <spacer flex="1"/>
              <label control="&NameField2.id;" value="&NameField2.label;"
                     accesskey="&NameField2.accesskey;"/>
              <hbox class="CardEditWidth" align="center">
                <textbox id="&NameField2.id;" flex="1"
                         oninput="GenerateDisplayName()"/>

                <!-- LOCALIZATION NOTE:
                     Fields for phonetic are disabled as default and can be
                     enabled by editing "mail.addr_book.show_phonetic_fields"
                  -->

                <spacer id="PhoneticSpacer2" flex="1" hidden="true"/>
                <label id="PhoneticLabel2" control="&PhoneticField2.id;"
                       value="&PhoneticField2.label;" hidden="true"/>
                <textbox id="&PhoneticField2.id;" flex="1" hidden="true"/>
              </hbox>
            </hbox>
            <hbox id="DisplayNameContainer" align="center">
              <spacer flex="1"/>
              <label control="DisplayName" value="&DisplayName.label;"
                     accesskey="&DisplayName.accesskey;" />
              <hbox class="CardEditWidth">
                <textbox id="DisplayName" flex="1"
                         oninput="DisplayNameChanged()"/>
              </hbox>
            </hbox>
            <hbox id="PreferDisplayNameContainer" align="center">
              <spacer flex="1"/>
              <hbox class="CardEditWidth">
                <checkbox id="preferDisplayName"
                          label="&preferDisplayName.label;"
                          accesskey="&preferDisplayName.accesskey;"/>
              </hbox>
            </hbox>

            <hbox id="NickNameContainer" align="center">
              <spacer flex="1"/>
              <label control="NickName" value="&NickName.label;"
                     accesskey="&NickName.accesskey;"/>
              <hbox class="CardEditWidth">
                <textbox id="NickName" flex="1"/>
              </hbox>
            </hbox>
            <hbox id="PrimaryEmailContainer" align="center">
              <spacer flex="1"/>
              <label control="PrimaryEmail" value="&PrimaryEmail.label;"
                     accesskey="&PrimaryEmail.accesskey;"/>
              <hbox class="CardEditWidth">
                <textbox id="PrimaryEmail" flex="1" class="uri-element"/>
              </hbox>
            </hbox>
            <hbox id="SecondaryEmailContainer" align="center">
              <spacer flex="1"/>
              <label control="SecondEmail" value="&SecondEmail.label;"
                     accesskey="&SecondEmail.accesskey;"/>
              <hbox class="CardEditWidth">
                <textbox id="SecondEmail" flex="1" class="uri-element"/>
              </hbox>
            </hbox>
            <hbox id="ScreenNameContainer" align="center">
              <spacer flex="1"/>
              <label class="text-link" value="&chatName.label;"
                     onclick="showChat();"/>
              <hbox class="CardEditWidth">
                <textbox id="ChatName" readonly="true" flex="1"
                         onclick="showChat();"/>
              </hbox>
            </hbox>
          </vbox> <!-- End of Name/Email -->
          <!-- Phone Number section -->
          <vbox id="PhoneNumbers">
            <hbox id="WorkPhoneContainer" align="center">
              <spacer flex="1"/>
              <label control="WorkPhone" value="&WorkPhone.label;"
                     accesskey="&WorkPhone.accesskey;" />
              <textbox id="WorkPhone" class="PhoneEditWidth"/>
            </hbox>
            <hbox id="HomePhoneContainer" align="center">
              <spacer flex="1"/>
              <label control="HomePhone" value="&HomePhone.label;"
                     accesskey="&HomePhone.accesskey;"/>
              <textbox id="HomePhone" class="PhoneEditWidth"/>
            </hbox>
            <hbox id="FaxNumberContainer" align="center">
              <spacer flex="1"/>
              <label control="FaxNumber" value="&FaxNumber.label;"
                     accesskey="&FaxNumber.accesskey;"/>
              <textbox id="FaxNumber" class="PhoneEditWidth"/>
            </hbox>
            <hbox id="PagerNumberContainer" align="center">
              <spacer flex="1"/>
              <label control="PagerNumber" value="&PagerNumber.label;"
                     accesskey="&PagerNumber.accesskey;"/>
              <textbox id="PagerNumber" class="PhoneEditWidth"/>
            </hbox>
            <hbox id="CellularNumberContainer" align="center">
              <spacer flex="1"/>
              <label control="CellularNumber" value="&CellularNumber.label;"
                     accesskey="&CellularNumber.accesskey;"/>
              <textbox id="CellularNumber" class="PhoneEditWidth"/>
            </hbox>
          </vbox> <!-- End of Phonenumbers -->
        </hbox> <!-- End of Name/Email/Phonenumbers -->
        <!-- Email Preferences -->
        <hbox align="center">
          <label control="PreferMailFormatPopup"
                 value="&PreferMailFormat.label;"
                 accesskey="&PreferMailFormat.accesskey;"/>

          <menulist id="PreferMailFormatPopup">
            <menupopup>
              <!-- 0,1,2 come from nsIAbPreferMailFormat in nsIAbCard.idl -->
              <menuitem value="0" label="&Unknown.label;"/>
              <menuitem value="1" label="&PlainText.label;"/>
              <menuitem value="2" label="&HTML.label;"/>
            </menupopup>
          </menulist>
        </hbox>
      </vbox> <!-- End of Name Tab -->

      <!-- ** Home Address Tab ** -->
      <vbox id="abHomeTab" >
        <hbox align="center">
          <spacer flex="1"/>
          <label control="HomeAddress" value="&HomeAddress.label;"
                 accesskey="&HomeAddress.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="HomeAddress" flex="1"/>
          </hbox>
        </hbox>
        <hbox align="center">
          <spacer flex="1"/>
          <label control="HomeAddress2" value="&HomeAddress2.label;"
                 accesskey="&HomeAddress2.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="HomeAddress2" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="HomeCityContainer" align="center">
          <spacer flex="1"/>
          <label control="HomeCity" value="&HomeCity.label;"
                 accesskey="&HomeCity.accesskey;"/>
          <hbox id="HomeCityFieldContainer" align="center" class="AddressCardEditWidth">
            <textbox id="HomeCity" flex="1"/>
          </hbox>
        </hbox>
        <hbox align="center">
          <spacer flex="1"/>
          <label control="HomeState" value="&HomeState.label;"
                 accesskey="&HomeState.accesskey;"/>
          <hbox align="center" class="AddressCardEditWidth">
            <textbox id="HomeState" flex="1"/>
            <spacer class="stateZipSpacer"/>
            <label control="HomeZipCode" value="&HomeZipCode.label;"
                   accesskey="&HomeZipCode.accesskey;"/>
            <textbox id="HomeZipCode" class="ZipWidth"/>
          </hbox>
        </hbox>
        <hbox align="center">
          <spacer flex="1"/>
          <label control="HomeCountry" value="&HomeCountry.label;"
                 accesskey="&HomeCountry.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="HomeCountry" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="WebPage2Container" align="center">
          <spacer flex="1"/>
          <label control="WebPage2" value="&HomeWebPage.label;"
                 accesskey="&HomeWebPage.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="WebPage2" flex="1" class="uri-element"/>
          </hbox>
        </hbox>
        <hbox id="birthdayField" align="center">
          <spacer flex="1"/>
          <label control="Birthday" value="&Birthday.label;"
                 accesskey="&Birthday.accesskey;"/>
          <hbox class="AddressCardEditWidth" align="center">
            <!-- NOTE: This datepicker is modified.
                 See abCardOverlay.js for details-->
            <datepicker id="Birthday" type="popup"/>
            <label value="&In.label;"/>
            <textbox id="BirthYear" maxlength="4"
                     placeholder="&Year.placeholder;" class="YearWidth" />
            <label control="Age" value="&Or.value;"/>
            <textbox id="Age" maxlength="4"
                     placeholder="&Age.placeholder;" class="YearWidth" />
            <label value="&YearsOld.label;"/>
            <spacer flex="1"/>
          </hbox>
        </hbox>
      </vbox>

      <!-- ** Business Address Tab ** -->
      <vbox id="abBusinessTab" >
        <hbox id="JobTitleDepartmentContainer" align="center">
          <spacer flex="1"/>
          <label control="JobTitle" value="&JobTitle.label;"
                 accesskey="&JobTitle.accesskey;"/>
          <hbox class="AddressCardEditWidth" align="center">
            <textbox id="JobTitle" flex="1"/>
            <label control="Department" value="&Department.label;"
                   accesskey="&Department.accesskey;"/>
            <textbox id="Department" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="CompanyContainer" align="center">
          <spacer flex="1"/>
          <label control="Company" value="&Company.label;"
                 accesskey="&Company.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="Company" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="WorkAddressContainer" align="center">
          <spacer flex="1"/>
          <label control="WorkAddress" value="&WorkAddress.label;"
                 accesskey="&WorkAddress.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="WorkAddress" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="WorkAddress2Container" align="center">
          <spacer flex="1"/>
          <label control="WorkAddress2" value="&WorkAddress2.label;"
                 accesskey="&WorkAddress2.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="WorkAddress2" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="WorkCityContainer" align="center">
          <spacer flex="1"/>
          <label control="WorkCity" value="&WorkCity.label;"
                 accesskey="&WorkCity.accesskey;"/>
          <hbox id="WorkCityFieldContainer" class="AddressCardEditWidth" align="center">
            <textbox id="WorkCity" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="WorkStateZipContainer" align="center">
          <spacer flex="1"/>
          <label control="WorkState" value="&WorkState.label;"
                 accesskey="&WorkState.accesskey;"/>
          <hbox class="AddressCardEditWidth" align="center">
            <textbox id="WorkState" flex="1"/>
            <spacer class="stateZipSpacer"/>
            <label control="WorkZipCode" value="&WorkZipCode.label;"
                   accesskey="&WorkZipCode.accesskey;"/>
            <textbox id="WorkZipCode" class="ZipWidth"/>
          </hbox>
        </hbox>
        <hbox id="WorkCountryContainer" align="center">
          <spacer flex="1"/>
          <label control="WorkCountry" value="&WorkCountry.label;"
                 accesskey="&WorkCountry.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="WorkCountry" flex="1"/>
          </hbox>
        </hbox>
        <hbox id="WebPage1Container" align="center">
          <spacer flex="1"/>
          <label control="WebPage1" value="&WorkWebPage.label;"
                 accesskey="&WorkWebPage.accesskey;"/>
          <hbox class="AddressCardEditWidth">
            <textbox id="WebPage1" flex="1" class="uri-element"/>
          </hbox>
        </hbox>
      </vbox>

      <!-- ** Other Tab ** -->
      <vbox id="abOtherTab" >
        <vbox id="customFields">
          <hbox flex="1" align="center">
            <label control="Custom1" value="&Custom1.label;"
                   accesskey="&Custom1.accesskey;"/>
            <textbox id="Custom1" flex="1"/>
          </hbox>
          <hbox flex="1" align="center">
            <label control="Custom2" value="&Custom2.label;"
                   accesskey="&Custom2.accesskey;"/>
            <textbox id="Custom2" flex="1"/>
          </hbox>
          <hbox flex="1" align="center">
            <label control="Custom3" value="&Custom3.label;"
                   accesskey="&Custom3.accesskey;"/>
            <textbox id="Custom3" flex="1"/>
          </hbox>
          <hbox flex="1" align="center">
            <label control="Custom4" value="&Custom4.label;"
                   accesskey="&Custom4.accesskey;"/>
            <textbox id="Custom4" flex="1"/>
          </hbox>
        </vbox>
        <label control="Notes" value="&Notes.label;"
               accesskey="&Notes.accesskey;"/>
        <textbox id="Notes" multiline="true" wrap="virtual" flex="1"/>
      </vbox>

      <!-- ** Chat Tab ** -->
      <hbox id="abChatTab">
        <vbox>
          <hbox id="GtalkContainer" align="center">
            <spacer flex="1"/>
            <label control="Gtalk" value="&Gtalk.label;"
                   accesskey="&Gtalk.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="Gtalk" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="AIMContainer" align="center">
            <spacer flex="1"/>
            <label control="AIM" value="&AIM.label;"
                   accesskey="&AIM2.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="AIM" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="YahooContainer" align="center">
            <spacer flex="1"/>
            <label control="Yahoo" value="&Yahoo.label;"
                   accesskey="&Yahoo.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="Yahoo" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="SkypeContainer" align="center">
            <spacer flex="1"/>
            <label control="Skype" value="&Skype.label;"
                   accesskey="&Skype.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="Skype" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="QQContainer" align="center">
            <spacer flex="1"/>
            <label control="QQ" value="&QQ.label;"
                   accesskey="&QQ.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="QQ" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="MSNContainer" align="center">
            <spacer flex="1"/>
            <label control="MSN" value="&MSN.label;"
                   accesskey="&MSN2.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="MSN" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="ICQContainer" align="center">
            <spacer flex="1"/>
            <label control="ICQ" value="&ICQ.label;"
                   accesskey="&ICQ.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="ICQ" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="XMPPContainer" align="center">
            <spacer flex="1"/>
            <label control="XMPP" value="&XMPP.label;"
                   accesskey="&XMPP.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="XMPP" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
          <hbox id="IRCContainer" align="center">
            <spacer flex="1"/>
            <label control="IRC" value="&IRC.label;"
                   accesskey="&IRC.accesskey;"/>
            <hbox class="CardEditWidth">
              <textbox id="IRC" flex="1" onchange="updateChatName();"/>
            </hbox>
          </hbox>
        </vbox>
      </hbox>

      <!-- ** Photo Tab ** -->
      <hbox id="abPhotoTab">
        <vbox align="center" id="PhotoContainer"
              style="height: 25ch; width: 25ch;"
              ondrop="doDropPhoto(event);"
              ondragenter="checkDropPhoto(event);"
              ondragover="checkDropPhoto(event);">
          <image id="photo" style="max-height: 25ch; max-width: 25ch;"/>
          <hbox id="PhotoDropTarget" flex="1" pack="center" align="center">
            <description>&PhotoDropTarget.label;</description>
          </hbox>
        </vbox>

        <vbox flex="1">
          <radiogroup id="PhotoType" oncommand="onSwitchPhotoType();">
            <vbox id="GenericPhotoContainer">
              <radio id="GenericPhotoType" value="generic" label="&GenericPhoto.label;"
                     accesskey="&GenericPhoto.accesskey;"/>
              <menulist id="GenericPhotoList" class="indent" flex="1"
                        oncommand="onSwitchPhotoType('generic', event);">
                <menupopup>
                  <menuitem label="&DefaultPhoto.label;" selected="true"
                            value="default"/>
                </menupopup>
              </menulist>
            </vbox>

            <vbox id="FilePhotoContainer">
              <radio id="FilePhotoType" value="file" label="&PhotoFile.label;"
                     accesskey="&PhotoFile.accesskey;"/>
              <hbox class="indent">
                <textbox id="PhotoFile" type="filefield" readonly="true"
                         maxlength="255" flex="1"/>
                <button id="BrowsePhoto"
                        oncommand="browsePhoto(event);"
                        label="&BrowsePhoto.label;"
                        accesskey="&BrowsePhoto.accesskey;"/>
              </hbox>
            </vbox>

            <vbox id="WebPhotoContainer">
              <radio id="WebPhotoType" value="web" label="&PhotoURL.label;"
                     accesskey="&PhotoURL.accesskey;"/>
              <hbox class="indent">
                <textbox id="PhotoURI" maxlength="255" flex="1"
                         placeholder="&PhotoURL.placeholder;"/>
                <button id="UpdatePhoto"
                        oncommand="onSwitchPhotoType('web', event);"
                        label="&UpdatePhoto.label;"
                        accesskey="&UpdatePhoto.accesskey;"/>
              </hbox>
            </vbox>
          </radiogroup>

          <hbox id="ProgressContainer" align="begin">
            <label id="PhotoStatus"/>
            <spacer flex="2"/>
            <html:progress id="PhotoDownloadProgress" value="0" max="100" hidden="hidden" flex="1"/>
          </hbox>
        </vbox>
      </hbox>
    </tabpanels>
  </tabbox>
</vbox>