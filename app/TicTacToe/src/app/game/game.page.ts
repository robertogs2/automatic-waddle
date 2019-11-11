import { Component, OnInit } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';
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
  player_turn = 1;
  waiting_player = true;
  
  available_positions:Number[] = [];

  id : any;

  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService) {

    this.id = setInterval(() => {
      if(this.player_turn == 2){
        this.available_positions = [];
        for(var i = 0; i < 9; i++){
          if(this.grid_busy[i] == 0){
            this.available_positions.push(i);
          }
        }
        if(this.available_positions.length != 0){
          console.log(this.available_positions);
          var rand_num = Math.floor(Math.random() * this.available_positions.length);
          console.log(rand_num);
          this.updateGrid(this.available_positions[rand_num], 2);
          this.player_turn = 1;
          this.turn = "Turn: " + this.connectionServices.getUser();
          
        }
      }
    }, 5000);//every second
  }


  onBack(){
    this.alertCtrl.create({
      header: 'Confirmation',
      message: 'Are you sure to quit the game?',
      buttons: [{text: 'Yes', handler: () => {this.router.navigate(['/home'])}}, {text: 'No', role: 'cancel'}]
    }).then(alertEl => {
      alertEl.present();
    });
    this.waiting_player = false; 
  }

  playComputer(){
    this.turn = "Turn: PC";
    this.available_positions = [];
    for(var i = 0; i < 9; i++){
      if(this.grid_busy[i] == 0){
        this.available_positions.push(i);
      }
    }
    if(this.available_positions.length != 0){
      var rand_num = Math.floor(Math.random() * this.available_positions.length);
      this.updateGrid(this.available_positions[rand_num], 2);
    }
  }

  updateGrid(position, symbol){
    //If it is empty
    if(this.grid_busy[position] == 0){
      if(symbol == 1){
        this.grid_images[position] = this.player_1_img;
      }
      else{
        this.grid_images[position] = this.player_2_img;
      }
      this.grid_busy[position] = symbol;
      if(symbol == 1){
        //this.playComputer();
        this.player_turn = 2;
        this.turn = "Turn: PC";
      }
    } 
  }

  ngOnInit() {
  }

}
