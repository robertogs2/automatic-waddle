import { Component } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';
import {HttpClient, HttpParams} from '@angular/common/http';


@Component({
  selector: 'app-home',
  templateUrl: 'home.page.html',
  styleUrls: ['home.page.scss'],
})
export class HomePage {
  inputIP: string = "";
  inputPort: string = "";
  dataObject: any;


  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService,
    public http: HttpClient)
   {}

  onConfirm(){
    console.log('confirm');
    if (this.inputIP.trim().length <= 0 || this.inputPort.trim().length <= 0) {

      this.alertCtrl.create({
        header: 'Error',
        message: 'Please enter a valid IP and port',
        buttons: [{text: 'Ok', role: 'cancel'}]
      }).then(alertEl => {
        alertEl.present();
      });
      console.log("Invalid port or ip");
      return;
    }

    this.connectionServices.setPort(this.inputPort);
    this.connectionServices.setIP(this.inputIP);

    this.http.get('http://' + this.connectionServices.getIP() + ':' + 
    this.connectionServices.getPort() + '/').subscribe((data:any) => {
      console.log(data);
      this.dataObject = data;
      if(this.dataObject['Status'] == "Ok"){
        this.router.navigate(['/setup']);
      }
      else{
        this.alertCtrl.create({
          header: 'Error',
          message: 'Connection failed',
          buttons: [{text: 'Ok', role: 'cancel'}]
        }).then(alertEl => {
          alertEl.present();
        });
      }
    });
    console.log('IP:', this.connectionServices.getIP(), 'Port:', this.connectionServices.getPort());
  }

}
