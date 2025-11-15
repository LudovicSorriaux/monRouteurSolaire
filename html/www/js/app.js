var $ = Dom7;

var app = new Framework7({
  name: 'Ma Routeur Control', // App name
  theme: 'auto', // Automatic theme detection
  el: '#app', // App root element
  
  // App store
  store: store,
  // App routes
  routes: routes,
  on: {
    // each object key means same name event handler
    popupOpen: function (popup) {
    },
    popupOpened: function (popup) {
      console.log('A popupOpened event for '+ popup.params.name); 
      if(popup.params.name === 'deleteUsr'){
        var frm = new FormData();
        validateOnServer('/getUsers',frm)
      }
    },
    popupClose: function (popup) {
    },
    popupClosed: function (popup) {
      console.log('A Popup closed Event for: '+popup.params.name)
      if(popup.params.name === 'addUsr'){
        if(wasInLogin) 
          app.loginScreen.open('#my-login-screen'); 
      } else if(popup.params.name === 'EndSession'){
        app.loginScreen.open('#my-login-screen'); 
      } else if(popup.params.name === 'logoff'){
        app.panel.close($('#panel-on-right'));
        app.loginScreen.open('#my-login-screen'); 
      }
    },
  },
});

$(document).on('page:mounted', function(e, page) {
  console.log("page mounted: "+page.name);
});
/*$(document).on('page:init', function (e, page) {
//  console.log("page init: " +page.name); 
});
$(document).on('page:reinit', function (e, page) {
//  console.log("page reInit: " +page.name); 
});
*/
$(document).on('page:beforein', function(e, page) {
  console.log("page before in: " +page.name);
  if(page.name === 'home'){
		console.log('-- STARTING page Pricipale Server Events --');
		sseRouteur.start();
  } else if (page.name === "parametres") {
    console.log ("stoping heartbeat")
    clearInterval(pingHome);                      // clear heartbeat on Page Principale
		console.log('-- STARTING page Paramètre Server Events --');
		sseParams.start();
  }
});
$(document).on('page:afterin', function(e, page) {
  console.log("page after in: " +page.name);
  if(page.name === 'home'){
    console.log ("starting heartbeat for every 5mm")
    pingHome = setInterval(initHomePage, 5*60*1000);   // set heartbeat every 5mn
  }
});
$(document).on('page:beforeout', function(e, page) {
  console.log("page before out: " +page.name);
  if(page.name === 'home'){
    console.log('Moving from home; stopping Page Principale SSE');
    sseRouteur.stop();
  } else if (page.name === "parametres") {
		console.log('Moving from parametres; stopping Page Paramètre SSE');
		sseParams.stop();
  } 
});
$(document).on('page:afterout', function(e, page) {
//  console.log("page after out: " +page.name);
});
$(document).on('page:beforeunmount', function(e, page) {
//  console.log("page before unmount: " +page.name);
});
$(document).on('page:beforeremove', function(e, page) {
//  console.log("page before remove: " +page.name);
});

// Start with Login sceeen
app.loginScreen.open('#my-login-screen')



// global variables 
var monRouteur = monRouteur || {};
var today = new Date();
var statusErrorMap = {
  '400' : "Server understood the request, but request content was invalid.",
  '401' : "Unauthorized access.",
  '403' : "Forbidden resource can't be accessed.",
  '405' : "Method Not Allowed",
  '500' : "Internal server error.",
  '503' : "Service unavailable."
};

var wasInLogin = false;

var sessID;
var ttl;
var expirationDate = 0;
var userName;
var passWord;
var timeoutPPID;

    // sse handlers 
var sseParams;
var sseRouteur;
var pingHome;     // for heartbeat

// monRouteur.session sigleton class to handle session infos
  monRouteur.Session = (function () {
    var instance;

    function init() {
        var sessionIdKey = "monRouteur-session";
        return {
        // Public methods and variables.
        set: function (sessionData) {
          window.localStorage.setItem(sessionIdKey, JSON.stringify(sessionData));
        },

        get: function () {
            var result = null;
            try {
            result = JSON.parse(window.localStorage.getItem(sessionIdKey));
          } catch(e){}
            return result;
        }
      };
    };

    return {
      getInstance: function () {
        if (!instance) {
          instance = init();
        }
        return instance;
      }
    };
    }());

  function storeUserInfos(){
    // Store the user infos and session. 
    today = new Date();
    expirationDate = today.getTime() + (ttl*1000);	// ttl is in sec where setTime is in millis  *1000
    console.log('In store Infos : now is: ' + today.getTime() + 'and expiration is: ' + expirationDate);
    monRouteur.Session.getInstance().set({
      username: userName,
      passord: passWord,
      sessionId: sessID,
      theExpirationDate: expirationDate,
    });
  }
  

  // Validate acction on server
  const validateOnServer = async function(url,formData) {
    var contentType;
    var result;
    var message;
    
    console.log('server call on: '+url+' parameters: '+app.utils.serializeObject(formData));

    try {
      let response = await fetch(url, { method: 'POST',
                                        body: formData,
                                        mode: 'cors',
                                        headers: {'Access-Control-Allow-Origin': '*',},
                                      });   
      if (response.ok) {
        console.log('HTTP status is successful');
        contentType = response.headers.get('Content-Type')
        if (contentType && contentType.includes('application/json')) {
          result = await response.json();
          onSuccess(result);
        } else {
          console.log('Error: Server response not json');
          message = "Error: Server response not json";
          throw new Error(message);
        }
      } else {                                  // not a 200 response
        console.log('HTTP status is not successful');
        if (response.status !=0 ) {                  // la reponse est valide
          if(response.status == "400") {
            response.text().then(function (text) {
              if (text.indexOf("Invalid Session") !== -1){				//400: Invalid Session
                console.log('THE SESSIONID EXPIRED NEXT CHANGE PAGE WILL GO TO LOGIN');
                app.popup.open($('#EndSession-popup'));
              } else if(text.indexOf("Invalid Request") !== -1){   //400: Invalid Request
                console.log("the POST request doesn't have good parameters");
                message="the request doesn't have the requested parameters";
                throw new Error(message);
              }
            });
          } else if (response.status === 500){  // test if error is json from server
            contentType = response.headers.get('Content-Type')
            if (contentType && contentType.includes('application/json')) {
              result = await response.json();
              onError(result)
            } else {
              response.text().then(text => 
                message = "Server Error => "+ response.status + ": "+ text
              )
              throw new Error(message);
            }
          } else {                              // not a 400 or 500 error
            message = "Error => "+ response.status + ": "+ statusErrorMap[response.status];
            if(!message){
              message="Unknown Error\n Response Status: " + response.status + "\n Response Text: " + response.responseText;
            }
            throw new Error(message);
          }
        } else {                                // Unknown Server Error
          message="Unknown Server Error";
          throw new Error(message);
        }
      }
    } catch (error) {
      console.error('Server Fetch ', error);
      app.dialog.alert(message,'Erreur !');

/* ===> Debug */ 
//      if(app.loginScreen.get($('#my-login-screen')).opened){
//        app.loginScreen.close($('#my-login-screen'));   // Close login screen
//      }
    }
  }
    function onSuccess(returnedData){
      var message ='';
      var popup;

      console.log("returned json is " + JSON.stringify(returnedData));

      message = "<H2>"+returnedData.status+"</H2>"
      message += "<br/>"+returnedData.message

      if(returnedData.status == "Processed"){
        message ='';    // call is sucessfull do nothing SSE message to follow !!! 
      } else if (returnedData.status == "Log in Successful"){       // login successful => main.html  
        userName = returnedData.username;	
        passWord = returnedData.password;	
        sessID = returnedData.sessionID;
        ttl = returnedData.ttl;
        storeUserInfos();
        $("#thename").val('');
        $("#thepassword").val('');
        $("#keepAlive").prop('checked', false);
        app.loginScreen.close('#my-login-screen');      // Close login screen
        popup = app.popup.get($('#EndSession-popup'))
        if(popup !== undefined){
          if(popup.opened)  popup.close()
        }
        if(app.panel.opened)  app.panel.close()
        message += "<br/>Your Session Time is : " + ttl + " seconds"
        app.dialog.alert(message,'Information');
        sseRouteur.start();                           // init données de la page principale 
      } else if (returnedData.status == "Log in Failed"){           // Login failed => retry
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "User Already Exist"){      // Registration user exist => retry
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "New User Created Succesfully"){     // Registration new user => main.html 
        if(returnedData.flgLogin == "true"){
          userName = returnedData.username;	
          passWord = returnedData.password;	
          sessID = returnedData.sessionID;
          ttl = returnedData.ttl;
          storeUserInfos();
          $("#newname").val('');
          $("#newpassword").val('');
          $("#newpassword2").val('');
          $("#adminpassword").val('');
            popup = app.popup.get($('#addUsr-popup'));
          if(popup !== undefined){
            if(popup.opened){
              app.popup.close($('#addUsr-popup'));
            }
          }
          popup = app.panel.get($('#panel-on-right'));
          if(popup !== undefined){
            if(popup.opened){
              app.panel.close($('#panel-on-right'));   // Close right panel
            }
          }
          popup = app.popup.get($('#my-login-screen'));
          if(popup !== undefined){
            if(popup.opened){
              app.loginScreen.close($('#my-login-screen'));   // Close login screen
            }
          }
          message += "br/>Your Session Time is : " + ttl + " seconds"
          app.dialog.alert(message,'Information');
        } else {			// add user but already login 
          app.dialog.alert('User Added but not loged in','Information');
        }  
          // Registration no room for new user => login
      } else if (returnedData.status == "No room for new user"){      
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "Bad AdminPassword"){      	// Registration bad adminPassword => retry	
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "Admin Password Updated"){  // change admin password updated => login   
        $("#oldadminpasswd").val('');
        $("#newadminpassword").val('');
        $("#adminpasswordchk").val('');
        popup = app.popup.get($('#adminUsrPasswd-popup'));
        if(popup !== undefined){
          if(popup.opened){
            app.popup.close($('#adminUsrPasswd-popup'));
            app.panel.close($('#panel-on-right'));
          }
        }
        app.dialog.alert(message,'Information');
      } else if (returnedData.status == "Bad Admin Password"){      // change admin password bad adminPassword => retry
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "User(s) Deleted"){      
        popup = app.popup.get($('#deleteUsr-popup'));
        if(popup !== undefined){
          if(popup.opened){
            app.popup.close($('#deleteUsr-popup'));
            app.panel.close($('#panel-on-right'));
          }
        }
        app.dialog.alert(message,'Information');
      } else if (returnedData.status == "User(s) Listed"){
        if(returnedData.hasOwnProperty('users')){      
          var html = "";
          $.each( returnedData.users, function ( i, obj ) {
            html += constructUserList(obj,i);
          });
          $('#usersList').empty();
          $('#usersList').html( html );
          $('#usersList').trigger('create');
          $('#usersList').listview('refresh');
        }
      } else if (returnedData.status == "No User(s) to Delete"){      
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "New Parameters Saved"){      
        message = "<br/>New Parameters saved !"
        app.dialog.alert(message,'Information');
        app.views.main.router.back()
      }	
    }
    function onError(result){
      var message;
      /*    {
              "exception" : 'parsererror','timeout','abort'
              "ErrorStatus" : 'The Status',
              "Correction" : 'This what to do ..',
            }
      */
        console.log("JSON Server Error is " + JSON.stringify(result));
        document.getElementById("routeurException").classList.add("hidden");
        document.getElementById("routeurErrorStatus").classList.add("hidden");
        document.getElementById("routeurErrorCorrection").classList.add("hidden");

        if(result.hasOwnProperty('exception')){
          if (result.exception=='parsererror'){
            message="Error.\nParsing JSON Request failed.";
          }else if(result.exception=='timeout'){
            message="Request Time out.";
          }else if(result.exception=='abort'){
            message="Request was aborted by the server";
          }else {
            message="Uncaught Server Error.";
          }
          document.getElementById("routeurException").classList.remove("hidden");
          document.getElementById("routeurException").val(message);
        }
        if(result.hasOwnProperty('ErrorStatus')){
          document.getElementById("routeurErrorStatus").classList.remove("hidden");
          document.getElementById("routeurErrorStatus").val(result.ErrorStatus);
        }
        if(result.hasOwnProperty('Correction')){
          document.getElementById("routeurErrorCorrection").classList.remove("hidden");
          document.getElementById("routeurErrorCorrection").val(result.Correction);
        }
        app.popup.open($('#Erreur-popup'));
        
        
        
/* ===> Debug  
      popup = app.popup.get($('#addUsr-popup'));
      if(popup !== undefined){
        if(popup.opened){
          app.popup.close($('#addUsr-popup'));
          app.panel.close();
        }
      }
      popup = app.popup.get($('#deleteUsr-popup'));
      if(popup !== undefined){
        if(popup.opened){
          app.popup.close($('#deleteUsr-popup'));
          app.panel.close();
        }
      }
      popup = app.popup.get($('#adminUsrPasswd-popup'));
      if(popup !== undefined){
        if(popup.opened){
          app.popup.close($('#adminUsrPasswd-popup'));
          app.panel.close();
        }
      }
      if(app.loginScreen.get($('#my-login-screen')).opened){
        app.loginScreen.close($('#my-login-screen'));   // Close login screen
        message += "br/>Your Session Time is : " + ttl + " seconds"
        app.dialog.alert(message,'Information');
        sse.start();
      }
*/        
    }

    // Login 
  $('#my-login-screen .login-button').on('click', function () {
    var valid = false
    var postData;

    valid = app.input.validateInputs($('#my-login-form'));
    if(valid === true){
      postData = "username="+$("#thename").val()+"&password="+$("#thepassword").val()+"&keepAlive="+$('#keepAlive').prop('checked')
      console.log(postData)
//      formData = app.form.convertToData($('#my-login-form'))
      validateOnServer("/logon",postData);
    } else {
      var text = ''
      if(!app.input.validate($("#thename")))
        text = 'Username invalid'
      if(!app.input.validate($("#thepassword"))){
        if(text.length === 0) 
          text = 'Password invalid'
        else
          text += '<br/>and Password invalid'
      }  
      app.dialog.alert(text,'Erreur !');
      console.log(text);
    }
  });

    // Logoff
  $('#logoffButton').on('click', function () {
    console.log("ask for loging off\n");
    sessID = "";
    userName = "";
    passWord = "";
    expirationDate = new Date();
    monRouteur.Session.getInstance().set({										// reset user credentials
      username: "",
      sessionId: "",
      xpirationDate: expirationDate
    });
  });

    // Open add user popup
  $('#addUserButton').on('click', function () {
    console.log("ask for opening add user popup\n");
    wasInLogin = true;
    app.loginScreen.close();
    app.popup.open($('#addUsr-popup'),false);
  });

      // Add New User 
  $('#addUsr').on('click', function () {
    var valid = false
    var postData

    valid = app.input.validateInputs($('#addUsr-form'));
    if(valid === true){
        var password = $("#newpassword").val();
        var password2 = $("#newpassword2").val();
      if(password.localeCompare(password2) != 0){
        var message = 'Password and Password verification are not the same !';
        $("#newpassword").val('');
        $("#newpassword2").val('');
        app.dialog.alert(message);
        console.log(message);
      } else {          // validation is OK so do things
        postData = "newname="+$("#newname").val()+"&newpassword="+$("#newpassword").val()+"&adminpassword="+$("#adminpassword").val()
        validateOnServer("/register",postData);
      }
    } else {            // validation isn't good
      var text = ''
      if(!app.input.validate($("#newname")))
        text = 'Username invalid'
      if(!app.input.validate($("#newpassword"))){
        if(text.length === 0) 
          text = 'Password invalid'
        else
          text += '<br/>and Password invalid'
      }  
      if(!app.input.validate($("#newpassword2"))){
        if(text.length === 0) 
          text = 'Check Password invalid'
        else
          text += '<br/>and Check Password invalid'
      }  
      if(!app.input.validate($("#adminpassword"))){
        if(text.length === 0) 
          text = 'Admin Password invalid'
        else
          text += '<br/>and Admin Password invalid'
      }  
      
      app.dialog.alert(text);
      console.log(text);
    }
  });

    // Change Admin Password 
  $('#adminUsrPasswd').on('click', function () {
    var valid = false;
    var doIt = true;
    var message = '';
    var postData;

    valid = app.input.validateInputs($('#adminUsrPasswd-form'));
    if(valid === true){
      var oldPassword = $("#oldadminpasswd").val();
      var newPassword = $("#newadminpassword").val();
      var checkPassword = $("#adminpasswordchk").val();
      if(oldPassword.localeCompare(newPassword) != 0){
        message += 'new Password and old Password are the same !<br/>';
        $("#oldadminpasswd").val('');
        $("#newadminpassword").val('');
        $("#adminpasswordchk").val('');
        doIt = false;
      }  
      if(newPassword.localeCompare(checkPassword) != 0){
        message += 'new Password and new Password verification are not the same !';
        $("#newadminpassword").val('');
        $("#adminpasswordchk").val('');
        doIt = false;
      }
      if(!doIt){
        app.dialog.alert(message);
        console.log(message);
      } else {          // validation is OK so do things
        postData = "oldadminpasswd="+$("#oldadminpasswd").val()+"&newadminpassword="+$("#newpanewadminpasswordssword").val()+"&adminpasswordchk="+$("#adminpasswordchk").val()
        validateOnServer("/adminPasswd",postData);
      }
    } else {            // validation isn't good
      if(!app.input.validate($("#adminpassword"))){
        app.dialog.alert('Admin Password invalid');
        console.log('Admin Password invalid');
      }  
      
    }
  });
  
    // Delete User(s) 
  $('#deleteUsr').on('click', function () {
    var valid = false
    var postData
    var i=1

    valid = app.input.validateInputs($('#deleteUsr-form'));
    if(valid === true){
      postData = "adminpassword="+$("#adminpasswordDU").val()
      $('.user').forEach(function(elem) {
        if(elem.checked){
          postData += "&user"+ i++ +"="+elem.val()
        }
        console.log(postData);
      });
      validateOnServer("/deleteUsers",postData);
    } else {            // validation isn't good
      if(!app.input.validate($("#adminpassword"))){
        app.dialog.alert('Admin Password invalid');
        console.log('Admin Password invalid');
      }  
      
    }
  });

    // on page delete users refresh users data from server
  function constructUserList(obj, index){
      var html = "";
      html += '<li>';
      html += '<label class="item-checkbox item-checkbox-icon-start item-content">';
      html += '<input type="checkbox" name="user[]" id="user'+index+'" value="'+obj.username+'>';
      html += '<i class="icon icon-checkbox"></i>';
      html += '<div class="item-inner">';
      html += '<div class="item-title">'+obj.username+'"</div>';
      html += '</div>';
      html += '</label>';
      html += '</li>';

      return html;
  }

  function switchClick(elem,checked){  
    var element = $('#'+elem).parent().attr('id')

    console.log("===>>    element is : "+$('#'+elem).attr('id')+" parent is :"+$('#'+elem).parent().attr('id')+" checked is:"+checked+" input is:"+$('#'+element+" input").attr('id')+" elemChecked before is:"+$('#'+element+" input").prop('checked'))
    $('#'+element+" input").prop('checked',checked)
    if (checked){
//      document.getElementById(element).getElementsByTagName("span")[0]
      console.log("   doing set switch checked")
      $('#'+element+" label").removeClass('switchUnchecked').addClass('switchChecked');
      $('#'+element+" span").removeClass('switchUnchecked').addClass('switchChecked');
      $('#'+element+" i").removeClass('switchUnchecked').addClass('switchChecked');
    } else {												
      console.log("   doing set switch unChecked")
      $('#'+element+" label").removeClass('switchChecked').addClass('switchUnchecked');
      $('#'+element+" span").removeClass('switchChecked').addClass('switchUnchecked');
      $('#'+element+" i").removeClass('switchChecked').addClass('switchUnchecked');
    }
  };

          // functions pour multi-page app
/* ------  Page Principale  ---- */
// pas de  "app.on('pageInit', function (page) {"  pour la page principale.
  
  // init page params via SSE for home page
  function initHomePage(){
    var formData = new FormData();

    formData.append('sess', sessID);
    validateOnServer('/setHomeSSEData', formData);
  }

  /* ----- creating SSE ---- */
  sseRouteur = new SSE('/routeurEvents', {
    onOpen: function (e) {
        console.log("Open SSE /routeurEvents");
        console.log(e);
    },
    onEnd: function (e) {
        console.log("Ending SSE /routeurEvents");
        console.log(e);
    },
    onError: function (e) {
        console.log("Could not connect to /routeurEvents");
        app.dialog.alert("Could not connect to SSE /routeurEvents try again later",'Erreur !')
    },
    onMessage: function (e) {
        console.log("Message from /routeurEvents");
        console.log(e);
        if(e.data.includes('hello!')){
          initHomePage();
        }
    },
    options: {
        forceAjax: false
    },
    events: {
      routeurData: function (e) {
        console.log("Message from /routeurEvents/routeurData");
        console.log(e);
        homeDataServer(e);
      }
    },
  });

  // receive SSE message from server to home page
  function homeDataServer(evt){
    var today = new Date();
    var timeLeft;
    var wordDiv = '';
    var doIt = false;
    var returnedData;

    console.log("homeDataServer");
    console.log(evt);
    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');
      /*    {
              "maisonW" : 2000,
              "edfW" : 1000,
              "pnxW" : 5000,
              "ballonW" : 200,
              "pacW" : 1000,
              "marcheForcee" : true/false,
              "modeManu" : true/false,
              "mfChauffeEau" : true/false,
              "mfPAC" : true/false,
              "temperatureEau" : 55,
              "pacSwitch" : true/false
            }
      */
      returnedData = JSON.parse(evt.data);
      console.log("serverEvent json is " + JSON.stringify(returnedData));

      if(returnedData.hasOwnProperty('maisonW')){
        if(!isNaN(returnedData.maisonW)){
          $('#maison').val(returnedData.maisonW);
        }
      }
      if(returnedData.hasOwnProperty('edfW')){
        if(!isNaN(returnedData.edfW)){
          (returnedData.edfW>=0)?$('#edf').val(returnedData.edfW):$('#edf').val(returnedData.edfW*(-1))
          ;
        }
      }
      if(returnedData.hasOwnProperty('from')){
        if(returnedData.from === 'LOAD'){                        //negatif donc vends à EDF
          $('#maison').removeClass('rouge').addClass('vert');   // autoConsommation en vert
          $('#edf').removeClass('rouge').addClass('vert');      // autoConsommation en vert
          console.log("iconDiv : "+($('#mainIconDiv').innerHTML === "")+", icontype : "+$('#mainIcon').attr('typeIcon')+", iconType = sad "+($('#mainIcon').attr('typeIcon')==='sad'))
          if( ($('#mainIconDiv').innerHTML === "") || ($('#mainIcon').attr('typeIcon')==='undefined') || ($('#mainIcon').attr('typeIcon')==='sad') ){                  // change mainIcon to happy face  
            $('#mainIconDiv').empty();
            wordDiv = ''                                          
            wordDiv += '<div class="grid_2 suffix_1" typeIcon="happy" id="mainIcon" style="margin-top: 15px;">'
            wordDiv += '  <svg height="50px" width="50px" style="color: lawngreen;" viewBox="0 0 26 26" xmlns="http://www.w3.org/2000/svg">'
            wordDiv += '    <path d="M13 .188C5.924.188.187 5.923.187 13S5.925 25.813 13 25.813c7.076 0 12.813-5.737 12.813-12.813C25.813 5.924 20.075.187 13 .187zm0 2c5.962 0 10.813 4.85 10.813 10.812S18.962 23.813 13 23.813C7.038 23.813 2.187 18.962 2.187 13C2.188 7.038 7.038 2.187 13 2.187zM9 9a2 2 0 1 0 0 4a2 2 0 0 0 0-4zm8 0a2 2 0 1 0 0 4a2 2 0 0 0 0-4zM5.75 16.094a8.83 8.83 0 0 0 7.25 3.75a8.829 8.829 0 0 0 7.25-3.75A12.374 12.374 0 0 1 13 18.437c-2.707 0-5.208-.874-7.25-2.343z" fill="currentColor"/>'
            wordDiv += '  </svg>'
            wordDiv += '</div>'
            $('#mainIconDiv').append(wordDiv).trigger( "create" );
          } 
          console.log("edfArrowDiv is vide: "+($('#edfArrowDiv').innerHTML === "")+", color : "+$('#edfArrow').attr('color')+", color = green "+($('#edfArrow').attr('color')==='green'))
          if($('#edfArrowDiv').innerHTML === ""){         // if empty or wrong color then recreate it 
            $('#edfArrowDiv').empty();
            wordDiv = ''                                          
            wordDiv += '<div class="arrow" id="edfArrow" item="edfVente" sens="right" color="green">'
            wordDiv += '  <span></span><span></span><span></span>'
            wordDiv += '</div>'
            $('#edfArrowDiv').append(wordDiv).trigger( "create" );
          } else if(!($('#edfArrow').attr('color')==='green')){
            $('#edfArrow').attr('sens','right')           // fleche vers EDF
            $('#edfArrow').attr('color','green')          // fleche en vert
            $('#edfArrow').attr('item','edfVente') 
          }   
        } else if(returnedData.from === 'GRID'){                 // positif donc consomme EDF
          $('#maison').removeClass('vert').addClass('rouge');     // Consommation EDF en rouge
          $('#edf').removeClass('vert').addClass('rouge');     // Consommation EDF en rouge
          console.log("iconDiv : "+($('#mainIconDiv').innerHTML === "")+", icontype : "+$('#mainIcon').attr('typeIcon')+", iconType = happy "+($('#mainIcon').attr('typeIcon')==='happy'))
          if( ($('#mainIconDiv').innerHTML === "") || ($('#mainIcon').attr('typeIcon')==='undefined') || ($('#mainIcon').attr('typeIcon')==='happy') ){                  // change mainIcon to sad face  
            $('#mainIconDiv').empty();
            wordDiv = ''                                          
            wordDiv += '<div class="grid_2 suffix_1" typeIcon="sad" id="mainIcon" style="margin-top: 15px;">'
            wordDiv += '  <svg height="50px" width="50px" style="color: orangered;" viewBox="0 0 64 64" xmlns="http://www.w3.org/2000/svg">'
            wordDiv += '    <path d="M32 2C15.428 2 2 15.428 2 32s13.428 30 30 30s30-13.428 30-30S48.572 2 32 2zm0 57.5C16.836 59.5 4.5 47.164 4.5 32S16.836 4.5 32 4.5c15.163 0 27.5 12.336 27.5 27.5c0 1.357-.103 2.69-.294 3.996c-.838-5.66-5.69-10.766-5.69-10.766s-5.828 6.113-5.828 12.375c0 6.353 6.393 7.996 9.708 4.937C53.251 52.488 43.431 59.5 32 59.5z" fill="currentColor"/>'
            wordDiv += '    <path d="M21.992 21.58c.541-.469-.971-2.061-1.414-1.674a14.232 14.232 0 0 1-11.693 3.133c-.578-.113-1.088 2.021-.385 2.156a16.417 16.417 0 0 0 13.492-3.615m23.121 1.307c-4.168.748-8.455-.4-11.691-3.133c-.443-.389-1.955 1.205-1.412 1.674a16.418 16.418 0 0 0 13.492 3.615c.703-.135.191-2.27-.389-2.156M38.074 47.33c-5.766-1.549-12.049-.428-16.93 3.014c-1.205.869 1.053 4.027 2.252 3.152c3.223-2.268 8.352-3.834 13.66-2.432c1.423.377 2.536-3.308 1.018-3.734" fill="currentColor"/>'
            wordDiv += '    <circle cx="38.498" cy="35" fill="currentColor" r="5"/>'
            wordDiv += '    <circle cx="15.498" cy="35" fill="currentColor" r="5"/>'
            wordDiv += '  </svg>'
            wordDiv += '</div>'
            $('#mainIconDiv').append(wordDiv).trigger( "create" );
          }
          console.log("edfArrowDiv is vide: "+($('#edfArrowDiv').innerHTML === "")+", color : "+$('#edfArrow').attr('color')+", color = red "+($('#edfArrow').attr('color')==='red'))
          if($('#edfArrowDiv').innerHTML === ""){         // if empty or wrong color then recreate it 
            $('#edfArrowDiv').empty();
            wordDiv = ''                                          // change mainIcon to sad face  
            wordDiv += '<div class="arrow" id="edfArrow" item="edfConso" sens="left" color="red">'
            wordDiv += '  <span></span><span></span><span></span>'
            wordDiv += '</div>'
            $('#edfArrowDiv').append(wordDiv).trigger( "create" );
          } else if(!($('#edfArrow').attr('color')==='red')) {
            $('#edfArrow').attr('sens','left')           // fleche vers EDF
            $('#edfArrow').attr('color','red')           // fleche en vert
            $('#edfArrow').attr('item','edfConso') 
          }
        } else {                                                 // pas d'info ne rien ecrire 
          $('#mainIconDiv').empty();
          $('#edfArrowDiv').empty();
        }
      }
      if(returnedData.hasOwnProperty('pnxW')){
        if(!isNaN(returnedData.pnxW)){
          $('#pnx').val(returnedData.pnxW);
          if(returnedData.pnxW > 0){ 
            if($('#pnxArrowDiv').innerHTML === ""){         // if empty then recreate it 
              $('#pnxArrowDiv').empty();
              wordDiv = ''                                          // change mainIcon to sad face  
              wordDiv += '<div class="arrow" id="pnxArrow" item="pnx" sens="right" color="green">'
              wordDiv += '  <span></span><span></span><span></span>'
              wordDiv += '</div>'
              $('#pnxArrowDiv').append(wordDiv).trigger( "create" );
            }
          } else {
            $('#pnxArrowDiv').empty();
          }
        }
      }
      if(returnedData.hasOwnProperty('ballonW')){
        if(!isNaN(returnedData.ballonW)){
          if((returnedData.ballonW == null)||(returnedData.ballonW == 0)){  // hide ballon div
            document.getElementById("ballonDiv").classList.add("hidden");
          } else {
            document.getElementById("ballonDiv").classList.remove("hidden");
            $('#ballon').val(returnedData.ballonW.toFixed(1));
          }
        } else {
          document.getElementById("ballonDiv").classList.add("hidden");
        }
      }
      if(returnedData.hasOwnProperty('pacW')){
        if(!isNaN(returnedData.pacW)){
          if((returnedData.pacW == null)||(returnedData.pacW == 0)){  // hide ballon div
            document.getElementById("PACDiv").classList.add("hidden");
          } else {                        // show ballon div 
            document.getElementById("PACDiv").classList.remove("hidden");
            $('#pac').val(returnedData.pacW);
          }
        } else {
          document.getElementById("PACDiv").classList.add("hidden");
        }
      }
      if(returnedData.hasOwnProperty('marcheForcee')){
        if(returnedData.marcheForcee == 0) {    // false
          $('#marcheForceLed').removeClass('ledOn').addClass('ledOff');
        } else {	                              // true
          $('#marcheForceLed').removeClass('ledOff').addClass('ledOn');
        }	
      }
      if(returnedData.hasOwnProperty('modeManu')){
        if(returnedData.modeManu == 0) {    // false
          $('#modeManuLed').removeClass('ledOn').addClass('ledOff');
        } else {	                              // true
          $('#modeManuLed').removeClass('ledOff').addClass('ledOn');
          if((returnedData.hasOwnProperty('marcheForcee')) && (returnedData.marcheForcee != 0) {    // true
            $('#marcheForceLed').removeClass('ledOn').addClass('ledOff');           // turn off MF while in manual mode
          }
        }	
      }
      if(returnedData.hasOwnProperty("pacSwitch")){
        if(returnedData.pacSwitch == 0) { // false
          $('#pacSWDiv').addClass("disabled")
        } else {
          $('#pacSWDiv').removeClass("disabled")
        }
      }
      if(returnedData.hasOwnProperty('mfChauffeEau')){
        switchClick('chauffeEauSwitch',returnedData.mfChauffeEau)
      }  
      if(returnedData.hasOwnProperty('mfPAC')){
        switchClick('pacSwitch',returnedData.mfPAC)
      }
      if(returnedData.hasOwnProperty('temperatureEau')){
        if(!isNaN(returnedData.temperatureEau)){
          if(returnedData.temperatureEau != -127){
            $('#tempEauDiv').removeClass("hidden")
            $('#tempEau').val(returnedData.temperatureEau.toFixed(2));
          }
        } else {  // hide temperature section
          $('#tempEauDiv').addClass("hidden")
        }
      }      
    }
  }

  /* --- home page elements --- */
  $('#chauffeEauSwitch').on('click', function () {
    var formData = new FormData();
    var askChecked;

    today = new Date();
    console.log("chauffeEauSwitch checked is : "+$(this).prop('checked'))
    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');
      askChecked = $(this).prop('checked')
      switchClick('chauffeEauSwitch',askChecked)
      formData.append('sess', sessID);
      formData.append('switch','chauffeEauSwitch')
      formData.append('value', askChecked)
      validateOnServer('/setSwitches',formData)
    }
  });

  $('#pacSwitch').on('click', function () {
    var formData = new FormData();
    var askChecked;

    today = new Date();
    console.log("pacSwitch checked is : "+$(this).prop('checked'))
    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');
      askChecked = $(this).prop('checked')
      switchClick('pacSwitch',askChecked)
      formData.append('sess', sessID);
      formData.append('switch','pacSwitch')
      formData.append('value',askChecked)
      validateOnServer('/setSwitches',formData)
    }
  });


/* ------  Autres Pages  ---- */

  app.on('pageInit', function (page) {
    console.log("page init: " +page.name); 
    if(page.name === 'home'){
      // nothing for Page Principale
    } else if (page.name === "parametres") {
    // fonctions pour page parametres 
      // init page via SSE for param page
      function initParamPage(){
        var formData = new FormData();

        formData.append('sess', sessID);
        validateOnServer('/setParamsSSEData', formData);
      }

      /* ----- creating SSE ---- */
       sseParams = new SSE('/paramEvents', {
        onOpen: function (e) {
            console.log("Open SSE to /paramEvents");
            console.log(e);
        },
        onEnd: function (e) {
            console.log("Ending SSE /paramEvents");
            console.log(e);
        },
        onError: function (e) {
            console.log("Could not connect to SSE /paramEvents");
            app.dialog.alert("Could not connect to SSE /paramEvents try again later",'Erreur !')
        },
        onMessage: function (e) {
            console.log("Message from /paramEvents");
            console.log(e);
            if(e.data.includes('hello!')){
              initParamPage();
            }
        },
        options: {
            forceAjax: false
        },
        events: {
          routeurParamsData: function (e) {
            console.log("Message from /paramEvents/routeurParamsData");
            console.log(e);
            paramDataServer(e);
          }
        },
      });

      // receive SSE message from server to param page
      function paramDataServer(evt){
        var today = new Date();
        var timeLeft;
        var returnedData;

        console.log("paramDataServer");
        console.log(evt);
        if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
          console.log('Session ttl expired: go to login diag');
          console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
          app.popup.open($('#EndSession-popup'));
        } else {													// session is still valid so perform data!
          timeLeft = 	(expirationDate - today.getTime())/1000;
          console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');
          /*    {
                  "heureBackup" : "20:00",
                  "secondBackup" : true/false,
                  "heureSecondBackup" : "20:00",
                  "sondeTemp" : true/false,
                  "tempEauMin" : 50,
                  "pacPresent" : true/false,
                  "puissPacOn" : 1000,
                  "puissPacOff" : 800,
                  "tempsOverProd" : 1,
                  "tempsMinPac" : 2,
                  "afficheur" : true/false,
                  "motionSensor" : true/false,
                  "volumeBallon" : 150,
                  "puissanceBallon" : 1500,
                }
          */
          returnedData = JSON.parse(evt.data);
          console.log("serverEvent json is " + JSON.stringify(returnedData));

          if(returnedData.hasOwnProperty('heureBackup')){
      /*        hour = (hour < 10 ? "0" : "") + hour;
            min = (min < 10 ? "0" : "") + min;
            displayTime = hour + ":" + min; 
            $("#formtime").value = displayTime;
      */
            $('#heureMarcheForce').val(returnedData.heureBackup);
          }
          if(returnedData.hasOwnProperty('secondBackup')){
            if(returnedData.secondBackup){                    // returnedData.secondBackup = true
                $("#marcheForceSuppSW").prop("checked", true)     
                $('#heureMarcheForceSec').prop("disabled",false)     
                $('#secondHeureDiv').removeClass("hidden")
            } else {                                          // returnedData.secondBackup = false
                $("#marcheForceSuppSW").prop("checked", false)  
                $('#heureMarcheForceSec').prop("disabled",true)     
                $('#secondHeureDiv').addClass("hidden")
            }
          }
          if(returnedData.hasOwnProperty('heureSecondBackup')){
            $('#heureMarcheForceSec').val(returnedData.heureSecondBackup);
          }
          if(returnedData.hasOwnProperty('sondeTemp')){
            if(returnedData.sondeTemp){                       // returnedData.sondeTemp = true
                $("#sondeTempSW").prop("checked", true);          
                $('#tempEauMin').prop("disabled",false)     
                $('#tempEauDiv').removeClass("hidden")
            } else {                                          // returnedData.sondeTemp = false
                $("#sondeTempSW").prop("checked", false);          
                $('#tempEauMin').prop("disabled",true)     
                $('#tempEauDiv').addClass("hidden")
            }
          }
          if(returnedData.hasOwnProperty('tempEauMin')){
            if(!isNaN(returnedData.tempEauMin)){
              $('#tempEauMin').val(returnedData.tempEauMin);
            }
          }
          if(returnedData.hasOwnProperty('pacPresent')){
            if(returnedData.pacPresent){                       // returnedData.pacPresent = true
                $("#pacPresentSW").prop("checked", true)          
                $('#puissPacOn').prop("disabled",false)     
                $('#puissPacOff').prop("disabled",false)     
                $('#tempsOverProd').prop("disabled",false)     
                $('#tempsPacMin').prop("disabled",false)     
                $('#pacPresentDiv').removeClass("hidden")
                $('#pacSWDiv').removeClass("disabled")

            } else {                                          // returnedData.pacPresent = false
                $("#pacPresentSW").prop("checked", false)          
                $('#puissPacOn').prop("disabled",true)     
                $('#puissPacOff').prop("disabled",true)     
                $('#tempsOverProd').prop("disabled",true)     
                $('#tempsPacMin').prop("disabled",true)     
                $('#pacPresentDiv').addClass("hidden")
                $('#pacSWDiv').addClass("disabled")
              }
          }
          if(returnedData.hasOwnProperty('puissPacOn')){
            if(!isNaN(returnedData.puissPacOn)){
              $('#puissPacOn').val(returnedData.puissPacOn);
            }
          }  
          if(returnedData.hasOwnProperty('puissPacOff')){
            if(!isNaN(returnedData.puissPacOff)){
              $('#puissPacOff').val(returnedData.puissPacOff);
            }
          }  
          if(returnedData.hasOwnProperty('tempsOverProd')){
            if(!isNaN(returnedData.tempsOverProd)){
              $('#tempsOverProd').val(returnedData.tempsOverProd);
            }
          }  
          if(returnedData.hasOwnProperty('tempsMinPac')){
            if(!isNaN(returnedData.tempsMinPac)){
              $('#tempsPacMin').val(returnedData.tempsMinPac);
            }
          }  
          if(returnedData.hasOwnProperty('afficheur')){
            if(returnedData.afficheur){                        // returnedData.afficheur = true
              if (!$('#afficheurSW').prop('checked')){        // if false need to change
                $("#afficheurSW").prop("checked", true);          
              }
            } else {                                           // returnedData.afficheur = false
              if ($('#afficheurSW').prop('checked')){         // if true need to change
                $("#afficheurSW").prop("checked", false);          
              }
            }
          }  
          if(returnedData.hasOwnProperty('motionSensor')){
            if(returnedData.motionSensor){                        // returnedData.motionSensor = true
              if (!$('#motionSensorSW').prop('checked')){        // if false need to change
                $("#motionSensorSW").prop("checked", true);          
              }
            } else {                                           // returnedData.motionSensor = false
              if ($('#motionSensorSW').prop('checked')){         // if true need to change
                $("#motionSensorSW").prop("checked", false);          
              }
            }
          }  
          if(returnedData.hasOwnProperty('volumeBallon')){
            if(!isNaN(returnedData.volumeBallon)){
              $('#volBallon').val(returnedData.volumeBallon);
            }
          }  
          if(returnedData.hasOwnProperty('puissanceBallon')){
            if(!isNaN(returnedData.puissanceBallon)){
              $('#puissanceBallon').val(returnedData.puissanceBallon);
            }
          }  
        }
      }

      /* ---- Actions ------ */
        // button
      $('#validParams').on('click', function () {
        var formData = new FormData();
        today = new Date();

        if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
          console.log('Session ttl expired: go to login diag');
          console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
          app.popup.open($('#EndSession-popup'));
        } else {													// session is still valid so perform data!
          timeLeft = 	(expirationDate - today.getTime())/1000;
          console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');
          formData.append('sess', sessID);

          formData.append('heureMarcheForce',$('#heureMarcheForce').val() )

          switchChecked = $('#marcheForceSuppSW').prop('checked')
          formData.append('marcheForceSuppSW', switchChecked)     // (value==0) ? val=false : val=true;
          if (switchChecked) {   // (value==0) ? val=unchecked : val=checked;
            formData.append('heureMarcheForceSec',$('#heureMarcheForceSec').val() )
          }    
          
          switchChecked = $('#sondeTempSW').prop('checked')
          formData.append('sondeTempSW', switchChecked)   // (value==0) ? val=false : val=true;
          if (switchChecked) {   // (value==0) ? val=unchecked : val=checked;
            formData.append('tempEauMin',$('#tempEauMin').val() )
          }    
          
          switchChecked = $('#pacPresentSW').prop('checked')
          formData.append('pacPresentSW', switchChecked)    // (value==0) ? val=false : val=true;
          if (switchChecked) {   // (value==0) ? val=unchecked : val=checked;
            formData.append('puissPacOn',$('#puissPacOn').val() )
            formData.append('puissPacOff',$('#puissPacOff').val() )
            formData.append('tempsOverProd',$('#tempsOverProd').val() )
            formData.append('tempsPacMin',$('#tempsPacMin').val() )
          }    
    
          formData.append('afficheurSW', $('#afficheurSW').prop('checked'))
          formData.append('motionSensorSW', $('#motionSensorSW').prop('checked'))

          formData.append('volBallon',$('#volBallon').val() )
          formData.append('puissanceBallon',$('#puissanceBallon').val() )

          validateOnServer('/setRouteurParams',formData)
        }
      });
      $('#annulParams').on('click', function () {
        app.views.current.router.back();
      });  
          // Switches 
      $('#marcheForceSuppSW').on('click', function () {
        if (!$('#marcheForceSuppSW').prop('checked')) {  // // (value==0) ? val=unchecked : val=checked;
          $('#secondHeureDiv').addClass("hidden")
          $('#heureMarcheForceSec').prop("disabled",true)
        } else {  // checked
          $('#secondHeureDiv').removeClass("hidden")
          $('#heureMarcheForceSec').prop("disabled",false)     
        }
      });
      $('#sondeTempSW').on('click', function () {
        if (!$('#sondeTempSW').prop('checked')) {  // // (value==0) ? val=unchecked : val=checked;
          $('#tempEauDiv').addClass("hidden")
          $('#tempEauMin').prop("disabled",true)
        } else {  // checked
          $('#tempEauDiv').removeClass("hidden")
          $('#tempEauMin').prop("disabled",false)     
      }
      });
      $('#pacPresentSW').on('click', function () {
          if (!$('#pacPresentSW').prop('checked')) {  // // (value==0) ? val=unchecked : val=checked;
            $('#pacPresentDiv').addClass("hidden")
            $('#puissPacOn').prop("disabled",true)     
            $('#puissPacOff').prop("disabled",true)     
            $('#tempsOverProd').prop("disabled",true)     
            $('#tempsPacMin').prop("disabled",true)     
          } else {  // checked
            $('#pacPresentDiv').removeClass("hidden")
            $('#puissPacOn').prop("disabled",false)     
            $('#puissPacOff').prop("disabled",false)     
            $('#tempsOverProd').prop("disabled",false)     
            $('#tempsPacMin').prop("disabled",false)     
            $('#pacSWDiv').removeClass("disabled")
          }
      });
      $('#afficheurSW').on('click', function () {
          if (!$('#afficheurSW').prop('checked')) {     // false unchecked
            $('#motionSensorSW').prop('checked', false)
            $('#motionSensorSW').prop("disabled",true)
          } else {                                      // true checked
            $('#motionSensorSW').prop("disabled",false)
          }
      });
    }
  });
 