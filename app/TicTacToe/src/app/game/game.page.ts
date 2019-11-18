import { Component, OnInit } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';
import {HttpClient, HttpParams} from '@angular/common/http';
@Component({
  selector: 'app-game',
  templateUrl: './game.page.html',
  styleUrls: ['./game.page.scss'],
})
export class GamePage implements OnInit {
  grid_images:string[] = ["assets/3.png", "assets/3.png", "assets/3.png",
                          "assets/3.png", "assets/3.png", "assets/3.png",
                          "assets/3.png", "assets/3.png", "assets/3.png"];
  grid_busy:Number[] = [0,0,0,
                        0,0,0,
                        0,0,0];
  player_1 = "Player 1: " + this.connectionServices.getUser() + "\t";
  player_2 = "Player 2: PC \t";
  player_1_img = "assets/" + this.connectionServices.getSymbol1().toString() + ".png";
  player_2_img = "assets/" + this.connectionServices.getSymbol2().toString() + ".png";
  turn = "Turn: " + this.connectionServices.getUser();
  turn_username = "";

  id : any;
  dataObject : any;

  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService,
    public http: HttpClient) {

    this.id = setInterval(() => {
      this.http.get('http://' + this.connectionServices.getIP() + ':' + 
      this.connectionServices.getPort() + '/game').subscribe((data:any) => {
      console.log(data);
      this.dataObject  = data;
      this.grid_busy = this.dataObject['Matrix'];
      this.turn = "Turn: " + this.dataObject['Turn'];
      this.player_1 = "Player 1: " + this.dataObject['Username0'] + "\t";
      this.player_2 = "Player 2: " + this.dataObject['Username1'] + "\t";
      this.player_1_img = "assets/" + this.dataObject['Symbol0'] + ".png";
      this.player_2_img = "assets/" + this.dataObject['Symbol1'] + ".png";

      for(var i = 0; i < 9; i++){
        this.grid_images[i] = "assets/" + this.grid_busy[i] + ".png";
      } 

      });
    }, 500);//every second
  }

  onPushedButton(position){
    console.log("Position: " + position);
    if(this.turn_username == this.connectionServices.getUser() && this.grid_busy[position] == 0){
      const params = new HttpParams().set('position', position).
                                      set('username', this.connectionServices.getUser());
      this.http.get('http://' + this.connectionServices.getIP() + ':' + 
      this.connectionServices.getPort() + '/game', {params}).subscribe((data:any) => {
        console.log(data);
        this.dataObject = data;
      });
    }
  }

  onBack(){
    this.alertCtrl.create({
      header: 'Confirmation',
      message: 'Are you sure to quit the game?',
      buttons: [{text: 'Yes', handler: () => {this.router.navigate(['/home'])}}, {text: 'No', role: 'cancel'}]
    }).then(alertEl => {
      alertEl.present();
    });
  }

  // playComputer(){
  //   this.turn = "Turn: PC";
  //   this.available_positions = [];
  //   for(var i = 0; i < 9; i++){
  //     if(this.grid_busy[i] == 0){
  //       this.available_positions.push(i);
  //     }
  //   }
  //   if(this.available_positions.length != 0){
  //     var rand_num = Math.floor(Math.random() * this.available_positions.length);
  //     this.updateGrid(this.available_positions[rand_num], 2);
  //   }
  // }

  // updateGrid(position, symbol){
  //   //If it is empty
  //   if(this.grid_busy[position] == 0){
  //     if(symbol == 1){
  //       this.grid_images[position] = this.player_1_img;
  //     }
  //     else{
  //       this.grid_images[position] = this.player_2_img;
  //     }
  //     this.grid_busy[position] = symbol;
  //     if(symbol == 1){
  //       //this.playComputer();
  //       this.player_turn = 2;
  //       this.turn = "Turn: PC";
  //     }
  //   } 
  // }

  ngOnInit() {
  }

}
